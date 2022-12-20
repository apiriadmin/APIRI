/*
 * fioapi_test.c
 * 
 * Copyright 2019 Q-Free Intelight <mike.gallagher@intelight-its.com>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 * 
 * 
 */


#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <signal.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <fio.h>

bool verbose_log = false;
bool quiet_log = false;

bool fio_signal = false;
void signal_handler(int sig)
{
	fio_signal = true;
}

int get_module_type(void)
{
	unsigned char buffer[16];
	
	// Use low-level method to check for FIO332 or FIOTS1
	int sp5_fd = open("/dev/sp5s", O_RDWR);
	if (sp5_fd < 0) {
		printf("Failed to open /dev/sp5s\n");
		return -1;
	}
	buffer[0] = 20;
	buffer[1] = 0x83;
	buffer[2] = 60;
	write(sp5_fd, &buffer[0], 3);
	struct pollfd sp5_poll_fd = { .fd = sp5_fd, .events = POLLIN|POLLPRI };
	if ((poll(&sp5_poll_fd, 1, 500) != 1)
		|| (read(sp5_fd, buffer, 4) < 4)) {
		printf("No response to module id cmd\n");
		close(sp5_fd);
		return -1;
	}
	close(sp5_fd);
	return buffer[3];
}

void print_io(unsigned char *outputs, unsigned char *inputs, int len)
{
	static unsigned char last_outputs[128];
	
	printf("Output pattern:");
	int ones = 0, flips = 0;
	for (int i=0; i<len; i++) {
		for(int j=0; j<8; j++) {
			if (outputs[i]&(0x1<<j))
				ones++;
			if ((outputs[i]&(0x1<<j)) != (last_outputs[i]&(0x1<<j)))
				flips++;
		}
		last_outputs[i] = outputs[i];
		printf(" %02x", outputs[i]);
	}
	printf("[%d][%d]\n", ones, flips);
	printf("Input pattern :");
	for (int i=0; i<len; i++) {
		printf(" %02x", inputs[i]);
	}
	printf("\n");
}

FIO_APP_HANDLE fio_handle;
FIO_DEV_HANDLE dev_handle;
int loop_test(FIO_DEVICE_TYPE type, unsigned char *outputs_plus, int len)
{
	unsigned char outputs_minus[FIO_INPUT_POINTS_BYTES] = {0};
	unsigned char inputs[FIO_INPUT_POINTS_BYTES] = {0};
	struct timespec to_sleep = { .tv_sec = 0, .tv_nsec = 200000000 };
	fio_hm_heartbeat(fio_handle);

	if (type == FIOTS1) {
		// Fix quirks in TS1 loopback cable 
		outputs_plus[9] &= 0x3f; outputs_plus[9] |= (outputs_plus[10] & 0xc0);
		outputs_plus[13] = outputs_plus[11];
		outputs_plus[14] = outputs_plus[12] & 0x3f;
	}
	
	fio_fiod_outputs_set(fio_handle, dev_handle, outputs_plus, outputs_minus, FIO_OUTPUT_POINTS_BYTES);
	fio_signal = false;
	nanosleep(&to_sleep, NULL);
	fio_fiod_frame_notify_register(fio_handle, dev_handle, 180, FIO_NOTIFY_ONCE);
	// Wait for next input frame response (notification signal?)
	sleep(1);
	if(fio_signal == false) {
		// Error no response frame after timeout
		printf("Response frame notification timeout occurred\n");
	}
	fio_fiod_inputs_get(fio_handle, dev_handle, FIO_INPUTS_RAW, inputs, sizeof(inputs));
	if (type == FIOTS1)
		// Fix quirks in TS1 loopback cable
		inputs[14] &= 0x3f;
		
	for (int j=0; j<len; j++) {
		if (inputs[j] != outputs_plus[j]) {
			// Test failed
			if (!quiet_log)
				print_io(outputs_plus, inputs, len);
			return -1;
		}
	}
	if (verbose_log)
		print_io(outputs_plus, inputs, len);
		
	return 0;
}

int main(int argc, char **argv)
{
	unsigned char output_map[FIO_OUTPUT_POINTS_BYTES];
	unsigned char outputs_plus[FIO_INPUT_POINTS_BYTES];
	int number_of_input_bytes = FIO_INPUT_POINTS_BYTES;
	FIO_FRAME_SCHD frame_schedules[3];
	struct sigaction act;
	int err = 0;
	FIO_DEVICE_TYPE fio_dev_type = FIO_UNDEF;

	printf("\nATC FIOAPI Loopback Test\n");

	if ((argc > 1) && (argv[1][0] == '-')) {
		if (argv[1][1] == 'v') {
			verbose_log = true;
			printf("Verbose logging enabled\n");
		} else if (argv[1][1] == 'q')
			quiet_log = true;
	}
	
	// Install signal handler for fio signal
	memset(&act, 0, sizeof(act));
	act.sa_handler = signal_handler;
	act.sa_flags = 0;
	sigaction(36/*FIO_SIGIO*/, &act, NULL);

	// Register with FIOAPI
	if ((fio_handle = fio_register()) < 0) {
		printf("Failed to fio_register, err(%d): %s\n",
			fio_handle, strerror(errno));
		return -1;
	}
	//printf("fio_register() successful\n");
	// Register health monitor service
	if (fio_hm_register(fio_handle, 100) != 0) {
		printf("Failed fio_hm_register(), err(%d): %s\n",
			fio_handle, strerror(errno));
		fio_deregister(fio_handle);
		return -1;
	}
	//printf("fio_hm_register() successful\n");
	
	// Determine io module type
	switch (get_module_type()) {
		case 1:
			// 332 module type
			if (verbose_log)
				printf("FIO_332 module type found\n");
			fio_dev_type = FIO332;
			number_of_input_bytes = 8;
			break;
		case 2:
			// TS1 module type
			if (verbose_log)
				printf("FIO_TS1 module type found\n");
			fio_dev_type = FIOTS1;
			break;
			
		default:
			if (!quiet_log)
				printf("No supported module type found\n");
			fio_deregister(fio_handle);
			return -1;
			break;
	}
	
	if ((dev_handle = fio_fiod_register(fio_handle, FIO_PORT_SP5, fio_dev_type)) < 0) {
		printf("fio_fiod_register(FIO_PORT_SP5, %s),err(%d),errno(%s)\n",
			(fio_dev_type == FIO332)?"FIO332":"FIOTS1", dev_handle, strerror(errno));
		fio_deregister(fio_handle);
		return -1;
	}
	//printf("dev_handle(%d) = fio_fiod_register() successful\n", dev_handle);
	memset(output_map, 0xff, sizeof output_map);
	FIO_BIT_CLEAR(output_map, 78);
	FIO_BIT_CLEAR(output_map, 79);
	if ((err = fio_fiod_outputs_reservation_set(fio_handle, dev_handle, output_map, sizeof(output_map))) < 0) {
		printf ("fio_fiod_outputs_reservation_set( fio_handle(%d), dev_handle(%d) ), "
			"errno(%s)\n", fio_handle, dev_handle, strerror(errno));
			goto error_dereg;
	}
	//printf("fio_fiod_outputs_reservation_set() successful\n");
	// Schedule FIO_INPUTS_RAW frame #52 (cancel frame #53)
	frame_schedules[0].req_frame = 52; frame_schedules[0].frequency = FIO_HZ_10;
	frame_schedules[1].req_frame = 53; frame_schedules[1].frequency = FIO_HZ_0;
	frame_schedules[2].req_frame = 55; frame_schedules[2].frequency = FIO_HZ_10;
	if (fio_fiod_frame_schedule_set(fio_handle, dev_handle, frame_schedules, 3) < 0) {
		printf ("fio_fiod_frame_schedule_set() failed, errno(%s)\n", strerror(errno));
		goto error_dereg;
	}
	// Enable this FIO module
	fio_fiod_enable(fio_handle, dev_handle);
	// Perform "walking 1" bit pattern test
  printf("walking 1 bit test...\n");
	for (int iter = 0; iter<number_of_input_bytes; iter++) {
		memset(outputs_plus, 0, sizeof outputs_plus);
		for(int j=0; j<8; j++) {
			outputs_plus[iter] = (1<<j);
			if (loop_test(fio_dev_type, outputs_plus, number_of_input_bytes) != 0) {
				printf("Error after %d iterations\n", iter+1);
				goto error_dereg;
			}
		}
	}
	// Perform muliple bit pattern test
  printf("random multiple bits test...\n");
	for (int iter=0; iter<100; iter++) {
		// Obtain a random bit pattern for outputs of length (8*FIO_OUTPUT_POINTS_BYTES)
		//getrandom(outputs_plus, sizeof outputs_plus, 0);
		int rand_fd = open("/dev/urandom", O_RDONLY);
		if (rand_fd < 0) {
			printf("Failed to open /dev/urandom\n");
			goto error_dereg;
		}
		read(rand_fd, outputs_plus, sizeof outputs_plus);
		close (rand_fd);
		
		if (loop_test(fio_dev_type, outputs_plus, number_of_input_bytes) != 0) {
			printf("Error after %d iterations\n", iter+1);
			goto error_dereg;
		}
	}
	
	printf("Test Passed\n");
	fio_fiod_disable(fio_handle, dev_handle);
	fio_fiod_deregister(fio_handle, dev_handle);
	fio_deregister(fio_handle);
	return 0;

error_dereg:
	fio_fiod_deregister(fio_handle, dev_handle);
	fio_deregister(fio_handle);
	return -1;

}
