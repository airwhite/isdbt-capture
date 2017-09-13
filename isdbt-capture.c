/* ISDB-T Capture. A DVB v5 API TS capture for Linux, for ISDB-TB 6MHz Latin American ISDB-T International.
 * Copyright (C) 2014 Rafael Diniz <rafael@riseup.net>
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 *
 */

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <signal.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/stat.h>


#define BUFFER_SIZE 4096
#define MAX_RETRIES 2


#include "dvb_resource.h"
#include "ring_buffer.h"

// Latin America channel assignments for ISDB-T International
uint64_t tv_channels[] =  
{ 
    /*  0 */ 0, /* index placeholders... */
    /*  1 */ 0, // never used for broadcasting
    /*  2 */ 0, // what to do?
    /*  3 */ 0, // what to do?
    /*  4 */ 0, // what to do?
    /*  5 */ 0, // future allocation for radio broadcasting
    /*  6 */ 0, // future allocation for radio broadcasting
    /*  7 */ 0,
    /*  8 */ 0,
    /*  9 */ 0,
    /* 10 */ 0,
    /* 11 */ 0,
    /* 12 */ 0,
    /* 13 */ 473142857, // Frequency of terrestrial digital broadcasting in Japan (13ch - 62ch)
    /* 14 */ 479142857,
    /* 15 */ 485142857,
    /* 16 */ 491142857,
    /* 17 */ 497142857,
    /* 18 */ 503142857,
    /* 19 */ 509142857,
    /* 20 */ 515142857,
    /* 21 */ 521142857,
    /* 22 */ 527142857,
    /* 23 */ 533142857,
    /* 24 */ 539142857,
    /* 25 */ 545142857,
    /* 26 */ 551142857,
    /* 27 */ 557142857,
    /* 28 */ 563142857,
    /* 29 */ 569142857,
    /* 30 */ 575142857,
    /* 31 */ 581142857,
    /* 32 */ 587142857,
    /* 33 */ 593142857,
    /* 34 */ 599142857,
    /* 35 */ 605142857,
    /* 36 */ 611142857,
    /* 37 */ 617142857,
    /* 38 */ 623142857,
    /* 39 */ 629142857,
    /* 40 */ 635142857,
    /* 41 */ 641142857,
    /* 42 */ 647142857,
    /* 43 */ 653142857,
    /* 44 */ 659142857,
    /* 45 */ 665142857,
    /* 46 */ 671142857,
    /* 47 */ 677142857,
    /* 48 */ 683142857,
    /* 49 */ 689142857,
    /* 50 */ 695142857,
    /* 51 */ 701142857,
    /* 52 */ 707142857,
    /* 53 */ 713142857,
    /* 54 */ 719142857,
    /* 55 */ 725142857,
    /* 56 */ 731142857,
    /* 57 */ 737142857,
    /* 58 */ 743142857,
    /* 59 */ 749142857,
    /* 60 */ 755142857,
    /* 61 */ 761142857,
    /* 62 */ 767142857,
    /* 63 */ 0,
    /* 64 */ 0,
    /* 65 */ 0,
    /* 66 */ 0,
    /* 67 */ 0,
    /* 68 */ 0,
    /* 69 */ 0,
};

/* global variables */
struct dvb_resource res;
FILE *ts = NULL;
FILE *player = NULL;
int adapter_no = -1;

// thread and ring buffer variables...
pthread_t output_thread_id;
pthread_mutex_t output_mutex;
pthread_cond_t output_cond;
struct ring_buffer output_buffer; 

int keep_reading;

int scan_channels(char *output_file, bool uhf_only, int adapter_no) 
{
  struct dvb_resource res;
  int layer_info = LAYER_FULL;

  FILE *fp = fopen(output_file, "w");
  int channel_id = 0;
  char adapter_name[64];
  sprintf(adapter_name, "/dev/dvb/adapter%d", adapter_no);

  for (int channel_counter = (uhf_only == true)? 13: 7;
       channel_counter <= 69; channel_counter++)
  {

      fprintf(stderr, "------------------------------------------------------------------\n");
      fprintf(stderr, "Channel = %d  Frequency = %lluHz\n", channel_counter, (unsigned long long) tv_channels[channel_counter]);

      dvbres_init(&res);

      if (dvbres_open(&res, tv_channels[channel_counter], adapter_name, layer_info) < 0)
      {
	  fprintf(stderr, "%s\n", res.error_msg);
	  exit(EXIT_FAILURE);
      }
      
      int i;
      fprintf(stderr, "Tuning");
      for (i = 0; i < MAX_RETRIES && dvbres_signallocked(&res) <= 0; i++)
      {
	  sleep(1);
	  fprintf(stderr, ".");
      }
      if (i == MAX_RETRIES)
      {
	  fprintf(stderr, "\nSignal not locked, next channel\n");
	  dvbres_close(&res); 
	  continue;
      }
      fprintf(stderr, "\nSignal locked!\n");

      channel_id++;
      fprintf(fp, "id %.2d name Canal_%.2d frequency %llu segment 1SEG\n", channel_id, channel_counter, (unsigned long long) tv_channels[channel_counter]);

      int power = dvbres_getsignalstrength(&res);
      if (power != 0)
	  fprintf(stderr, "Signal power = %d\n", power);
      
      int snr = dvbres_getsignalquality(&res);
      if (snr != 0)
	  fprintf(stderr, "Signal quality = %d\n", snr);
      
      dvbres_close(&res);
  }

  fclose(fp);

  return 0;
}

void finish(int s){

    char fifo_file[64];

    fprintf(stderr, "\nExiting...\n");

    keep_reading = 0;

    pthread_join(output_thread_id, NULL);

    dvbres_close(&res);

    if (ts)
	fclose(ts);

    if (player) {
	fclose(player);
	sprintf(fifo_file, "/tmp/out%d.ts", adapter_no);
	remove(fifo_file);
	fprintf(stderr, "rm %s\n", fifo_file);
    }

    exit(EXIT_SUCCESS); 
}

void *output_thread(void *nothing)
{
    void *addr;
    int bytes_read;
    char buffer[BUFFER_SIZE];
    
    while (keep_reading)
    {
	bytes_read = read(res.dvr, buffer, BUFFER_SIZE);
	if (bytes_read <= 0)
	{
	    struct pollfd fds[1];
	    fds[0].fd = res.dvr;
	    fds[0].events = POLLIN;
	    poll(fds, 1, -1);
	    continue;
	}
	
    try_again_write:
	if (ring_buffer_count_free_bytes (&output_buffer) >= bytes_read)
	{ 
	    pthread_mutex_lock(&output_mutex);
            addr = ring_buffer_write_address (&output_buffer);
            memcpy(addr, buffer, bytes_read);
            ring_buffer_write_advance(&output_buffer, bytes_read);
	    pthread_cond_signal(&output_cond);
            pthread_mutex_unlock(&output_mutex);
	}
        else
        {
	    fprintf(stderr, "Buffer full, nich gut...\n");
	    pthread_mutex_lock(&output_mutex);
            pthread_cond_wait(&output_cond, &output_mutex);
            pthread_mutex_unlock(&output_mutex);
            goto try_again_write;
        }
    }

    return NULL;
}

int main (int argc, char *argv[])
{
    uint64_t freq = 599142000ULL;
    int bytes_written;
    char buffer[BUFFER_SIZE];
    char output_file[512];
    char scan_file[512];
    int layer_info = LAYER_FULL;
    bool scan_mode = false, info_mode = false, player_mode = false, tsoutput_mode = false;
    char temp_file[] = "/tmp/out.ts";
    char player_cmd[256];
    char adapter_name[64];

    int opt;
    void *addr;


    pthread_mutex_init(&output_mutex, NULL); 
    pthread_cond_init(&output_cond, NULL); 
    ring_buffer_create(&output_buffer, 28); 
    
    signal (SIGINT,finish);
    
    fprintf(stderr, "isdbt-capture by Rafael Diniz -  rafael (AT) riseup (DOT) net\n");
    fprintf(stderr, "License: GPLv3+\n\n");

    if (argc < 2)
    {
    manual:
	fprintf(stderr, "Usage modes: \n%s -a adapter_number -c channel_number -p player -o output.ts [-l layer_info]\n", argv[0]);
	fprintf(stderr, "%s [-s channels.txt -a adapter_number]\n", argv[0]);
	fprintf(stderr, "%s [-i]\n", argv[0]);
	fprintf(stderr, "\nOptions:\n");
	fprintf(stderr, " -c                Channel number (7-69) (Mandatory).\n");
	fprintf(stderr, " -o                Output TS filename (Optional).\n");
	fprintf(stderr, " -a                Adapter number (0-n) (Optional).\n");
	fprintf(stderr, " -p                Choose a player to play the selected channel (Eg. \"mplayer -vf yadif\" or \"vlc\") (Optional).\n");
	fprintf(stderr, " -l                Layer information. Possible values are: 0 (All layers), 1 (Layer A), 2 (Layer B), 3 (Layer C) (Optional).\n\n");
	fprintf(stderr, " -s channels.cfg   Scan for channels, store them in a file and exit. with -a option.\n");
        fprintf(stderr, " -i                Print ISDB-T device information and exit.\n");
	fprintf(stderr, "\nTo quit press 'Ctrl+C'.\n");
	exit(EXIT_FAILURE);
    }

    while ((opt = getopt(argc, argv, "io:a:c:l:s:p:")) != -1) 
    {
        switch (opt) 
        {
	case 'i':
	    info_mode = true;
	    break;
	case 'o':
	    tsoutput_mode = true;
	    strcpy(output_file, optarg);
	    break;
	case 'a':
	    adapter_no = atoi(optarg);
	    fprintf(stderr, "/dev/dvb/adapter%d selected.\n", adapter_no);
	    break;
	case 'c':
	    freq = tv_channels[atoi(optarg)];
	    fprintf(stderr, "Frequency %llu (CH %d) selected.\n", (unsigned long long) freq, atoi(optarg));
	    break;
	case 'l':
	    layer_info = atoi(optarg);
	    fprintf(stderr, "Layer %d selected\n", layer_info);
	    break;
	case 's':
	    scan_mode = true;
	    strcpy(scan_file, optarg);
	    break;
	case 'p':
	    player_mode = true;
	    strcpy(player_cmd, optarg);
	    break;
	default:
	    goto manual;
	}
    }
    
    if (info_mode == true)
    {
	memset(buffer, 0, BUFFER_SIZE);
	fprintf(stderr, "Devices information:\n");
	if (dvbres_listdevices(&res, buffer, BUFFER_SIZE) < 0)
	{
	    fprintf(stderr, "%s\n\n", res.error_msg);
	}
	else
	{
	    fprintf(stderr, "%s\n\n", buffer);
	}
    }

    if(scan_mode == true && adapter_no != -1)
    {
	fprintf(stderr, "Scan information:\n");
	scan_channels(scan_file, true, adapter_no);
    }

    if (info_mode == true || scan_mode == true)
    {
	exit(EXIT_SUCCESS);
    }


    fprintf(stderr, "Initializing DVB structures.\n");
    dvbres_init(&res);

    fprintf(stderr, "Opening DVB devices.\n");
    if (adapter_no == -1) {
	if (dvbres_open(&res, freq, NULL, layer_info) < 0)
	{
	    fprintf(stderr, "%s\n", res.error_msg);
	    exit(EXIT_FAILURE);
	}
    } else {
	sprintf(adapter_name, "/dev/dvb/adapter%d", adapter_no);
	if (dvbres_open(&res, freq, adapter_name, layer_info) < 0)
	{
	    fprintf(stderr, "%s\n", res.error_msg);
	    exit(EXIT_FAILURE);
	}
    }

    int i;
    fprintf(stderr, "Tuning.");
    for (i = 0; i < MAX_RETRIES && dvbres_signallocked(&res) <= 0; i++)
    {
	sleep(1);
	fprintf(stderr, ".");
    }
    if (i == MAX_RETRIES)
    {
	fprintf(stderr, "\nSignal not locked.\n");
	dvbres_close(&res);
	return -1;
    }
    fprintf(stderr, "\nSignal locked!\n");
    
    
    if (player_mode == true)
    {
	if (adapter_no != -1) sprintf(temp_file, "/tmp/out%d.ts", adapter_no);
	unlink(temp_file);
	mkfifo(temp_file, S_IRWXU | S_IRWXG | S_IRWXO);

	char cmd[512];
	sprintf(cmd, "%s %s &", player_cmd, temp_file);
	fprintf(stderr, "Running: %s\n", cmd);
	system(cmd);
	
	player = fopen(temp_file, "w");
	if (player == NULL)
	{
	    fprintf(stderr, "Error opening fifo: %s.\n", temp_file);
	    exit(EXIT_FAILURE);
	}
	else
	{
	    fprintf(stderr, "Fifo %s opened.\n", temp_file);
	}

    }

    if (tsoutput_mode == true)
    {
	ts = fopen(output_file, "w");
	if (ts == NULL)
	{
	    fprintf(stderr, "Error opening file: %s.\n", output_file);
	    exit(EXIT_FAILURE);
	}
	else
	{
	    fprintf(stderr, "File %s opened.\n", output_file);
	}
    }


    int power = dvbres_getsignalstrength(&res);
    if (power != 0)
	fprintf(stderr, "Signal power = %d\n", power);

    int snr = dvbres_getsignalquality(&res);
    if (snr != 0)
	fprintf(stderr, "Signal quality = %d\n", snr);

    // starting output thread
    keep_reading = 1;
    pthread_create(&output_thread_id, NULL, output_thread, NULL);

    while (1) 
    {
    try_again_read:
	if (ring_buffer_count_bytes(&output_buffer) >= BUFFER_SIZE)
	{
	    pthread_mutex_lock(&output_mutex);
	    addr = ring_buffer_read_address(&output_buffer);
	    memcpy(buffer, addr, BUFFER_SIZE);
	    ring_buffer_read_advance(&output_buffer, BUFFER_SIZE);
	    pthread_cond_signal(&output_cond);
	    pthread_mutex_unlock(&output_mutex);
	}
	else
	{	    
	    pthread_mutex_lock(&output_mutex);
	    pthread_cond_wait(&output_cond, &output_mutex);
	    pthread_mutex_unlock(&output_mutex);
	    goto try_again_read;
	}

	if (tsoutput_mode == true)
	    bytes_written = fwrite(buffer, 1, BUFFER_SIZE, ts);

	if (player_mode == true)
	    bytes_written = fwrite(buffer, 1, BUFFER_SIZE, player);

	if (bytes_written != BUFFER_SIZE)
	    fprintf(stderr, "bytes_written = %d != bytes_read = %d\n", bytes_written, BUFFER_SIZE);

	// small trick to not call the api too much
	if (!(i++ % 100))
	{
	    power = dvbres_getsignalstrength(&res);
	    fprintf(stderr, "Signal power = %d%%\r", power);
	    
	}
	
    }
    
    return EXIT_SUCCESS;
}
