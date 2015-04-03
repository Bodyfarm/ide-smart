/*
 *  ide-smart 1.4 - IDE S.M.A.R.T. cheking tool
 *  Copyright (C) 1998-2001 Ragnar Hojland Espinosa <ragnar@ragnar-hojland.com>
 *                1998      Gadi Oxman <gadio@netvision.net.il>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <linux/hdreg.h>
#include <linux/types.h>
#include <getopt.h>
#include <errno.h>

#define NR_ATTRIBUTES	30

#ifndef TRUE
#define TRUE 1
#endif

typedef struct threshold_s {
   __u8		id;
   __u8		threshold;
   __u8		reserved[10];
} __attribute__ ((packed)) threshold_t;


typedef struct thresholds_s {
   __u16	revision;
   threshold_t	thresholds[NR_ATTRIBUTES];
   __u8		reserved[18];
   __u8		vendor[131];
   __u8		checksum;
} __attribute__ ((packed)) thresholds_t;


typedef struct value_s {
   __u8		id;
   __u16	status;
   __u8		value;
   __u8		vendor[8];
} __attribute__ ((packed)) value_t;


typedef struct values_s {
   __u16	revision;
   value_t	values[NR_ATTRIBUTES];
   __u8		offline_status;
   __u8		vendor1;
   __u16	offline_timeout;
   __u8		vendor2;
   __u8		offline_capability;
   __u16	smart_capability;
   __u8		reserved[16];
   __u8		vendor[125];
   __u8		checksum;
} __attribute__ ((packed)) values_t;


struct {
   __u8		value;
   char		*text;
} offline_status_text[] = {
   { 0x00,	"NeverStarted" },
   { 0x02,	"Completed"    },
   { 0x04,	"Suspended"    },
   { 0x05,	"Aborted"      },
   { 0x06,	"Failed"       },
   { 0, 0 }
};


struct {
   __u8		value;
   char		*text;
} smart_command[] = {
   { SMART_ENABLE,	 	"SMART_ENABLE"		  },
   { SMART_DISABLE,	 	"SMART_DISABLE"           },
   { SMART_IMMEDIATE_OFFLINE,	"SMART_IMMEDIATE_OFFLINE" },
   { SMART_AUTO_OFFLINE,	"SMART_AUTO_OFFLINE"      },
};


/* Index to smart_command table, keep in order */
enum SmartCommand
{ 
   SMART_CMD_ENABLE,
   SMART_CMD_DISABLE, 
   SMART_CMD_IMMEDIATE_OFFLINE, 
   SMART_CMD_AUTO_OFFLINE 
};
   

char *get_offline_text (int status)
{
   int i;

   for (i = 0; offline_status_text[i].text; i++) {
      if (offline_status_text[i].value == status) {
	 return offline_status_text[i].text;
      }
   }
   
   return "unknown";
}


int smart_read_values (int fd, values_t* values)
{   
   __u8 args[4 + 512] = {WIN_SMART, 0, SMART_READ_VALUES, 1, };

   if (ioctl(fd, HDIO_DRIVE_CMD, &args)) {
      int e = errno;
      perror ("SMART_READ_VALUES");
      return e;
   }

   memcpy (values, args + 4, 512);
   return 0;
}


int values_not_passed (values_t *p, thresholds_t *t)
{
   value_t *value = p->values;
   threshold_t *threshold = t->thresholds;
   int failed = 0;
   int passed = 0;
   int i;
   
   for (i = 0; i < NR_ATTRIBUTES; i++) {
      if (value->id && threshold->id && value->id == threshold->id) {
	 if (value->value <= threshold->threshold) {
	    ++failed;
	 } else {
	    ++passed;
	 }
      }
      
      ++value;
      ++threshold;
   }
   
   return (passed ? -failed : 2);
}


void print_value (value_t *p, threshold_t *t)
{
   printf ("Id=%3d  Status=%2d  {%s  %s}  Value=%3d  Threshold=%3d  %s\n", 
	   p->id, p->status,
	   p->status & 1 ? "Prefailure" : 
	                   "Advisory  ",
	   p->status & 2 ? "Online " :
	                   "OffLine",
	   p->value, t->threshold,
	   p->value > t->threshold ? "Passed" : "Failed");
}


void print_values (values_t *p, thresholds_t *t)
{
   value_t *value = p->values;
   threshold_t *threshold = t->thresholds;
   int i;

   for (i = 0; i < NR_ATTRIBUTES; i++) {
      if (value->id && threshold->id && value->id == threshold->id) {
	 print_value (value++, threshold++);
      }
   }
   
   printf ("OffLineStatus=%d {%s}, AutoOffLine=%s, OffLineTimeout=%d minutes\n",
	  p->offline_status, get_offline_text(p->offline_status & 0x7f),
	  (p->offline_status & 0x80 ? "Yes" : "No"),
	  p->offline_timeout / 60);
   printf ("OffLineCapability=%d {%s %s %s}\n",  p->offline_capability,
	  p->offline_capability & 1 ? "Immediate" : "",
	  p->offline_capability & 2 ? "Auto" : "",
	  p->offline_capability & 4 ? "AbortOnCmd" : "SuspendOnCmd");
   printf ("SmartRevision=%d, CheckSum=%d, SmartCapability=%d {%s %s}\n",
	  p->revision, p->checksum, p->smart_capability,
	  p->smart_capability & 1 ? "SaveOnStandBy" : "",
	  p->smart_capability & 2 ? "AutoSave" : "");
}


void print_thresholds (thresholds_t *p)
{
   threshold_t *threshold = p->thresholds;
   int i;

   printf ("\n");
   printf ("SmartRevision=%d\n", p->revision);

   for (i = 0; i < NR_ATTRIBUTES; i++) {
      if (threshold->id) {
	 printf ("Id=%3d, Threshold=%3d\n", threshold->id, threshold->threshold);
      }
      ++threshold;
   }
   
   printf ("CheckSum=%d\n", p->checksum);
}


int smart_cmd_simple (int fd, enum SmartCommand command, __u8 val0, char show_error)
{
   __u8 args[4] = { WIN_SMART, val0, smart_command[command].value, 0 };
   int e = 0;
   
   if (ioctl (fd, HDIO_DRIVE_CMD, &args)) {
      e = errno;
      if (show_error) {
	 perror (smart_command[command].text);
      }
   }
   
   return e;
}
   

int smart_read_thresholds (int fd, thresholds_t *thresholds)
{
   __u8 args[4 + 512] = {WIN_SMART, 0, SMART_READ_THRESHOLDS, 1, };

   if (ioctl (fd, HDIO_DRIVE_CMD, &args)) {
      int e = errno;
      perror("SMART_READ_THRESHOLDS");
      return e;
   }
   
   memcpy (thresholds, args + 4, 512);
   return 0;
}


void show_version()
{
   printf ("ide-smart 1.4 - FREE Software with NO WARRANTY\n");
   printf ("(C) 2001 Ragnar Hojland Espinosa <ragnar@ragnar-hojland.com>\n");
}


void show_help()
{
   printf ("Usage: ide-smart [DEVICE] [OPTION]\n"
	   "       -d, --device=DEVICE  Select device DEVICE\n"
	   "       -i, --immediate      Perform immediately offline tests\n"
	   "       -q, --quiet-check    Returns the number of failed tests\n"
	   "       -1, --auto-on        Turn on automatic offline tests\n"
	   "       -0, --auto-off       Turn off automatic offline tests\n"
	   "       -h, --help\n"
	   "       -V, --version\n");
}


int main (int argc, char *argv[])
{
   char *device = NULL;
   int command = -1;

   int o, longindex;
   int retval = 0;
   
   const struct option longopts[] = {
      { "device",   	required_argument, 0, 'd' },
      { "immediate",	no_argument,       0, 'i' },
      { "quiet-check", 	no_argument, 	   0, 'q' },
      { "auto-on",   	no_argument,       0, '1' },
      { "auto-off",  	no_argument,       0, '0' },
      { "help",      	no_argument,       0, 'h' },
      { "version",   	no_argument,       0, 'V' },
      { 0, 0, 0, 0 }
   };
   
   while ((o=getopt_long(argc, argv, "d:iq10hV",longopts,&longindex)) != -1)
   switch (o) {
    case 'd':
      device = optarg;
      break;
    case 'q':
      command = 3;
      break;
    case 'i':
      command = 2;
      break;
    case '1':
      command = 1;
      break;
    case '0':
      command = 0;
      break;
    case 'h':
      show_help();
      return 0;
    case 'V':
      show_version();
      return 0;      
    default:
      fprintf (stderr, "Try `smart-ide --help' for more information.\n");
      return 1;
   }
   
   if (optind < argc) {
      device = argv[optind];
   }
   
   if (!device) {
      show_version();
      show_help();
      return 1;
   }

   if (1) {
      thresholds_t thresholds;
      values_t values;      
      int fd = open (device, O_RDONLY);
      
      if (fd < 0) {
	 perror("Couldn't open device");
	 return 2;
      }
      
      if (smart_cmd_simple (fd, SMART_CMD_ENABLE, 0, TRUE)) {
	 return 2;
      }
      
      switch (command) {
       case 0:
	 retval = smart_cmd_simple (fd, SMART_CMD_AUTO_OFFLINE, 0, TRUE);
	 break;
       case 1:
	 retval = smart_cmd_simple (fd, SMART_CMD_AUTO_OFFLINE, 0xF8, TRUE);
	 break;
       case 2:
	 retval = smart_cmd_simple (fd, SMART_CMD_IMMEDIATE_OFFLINE, 0, TRUE);
	 break;
       case 3:
	 smart_read_values (fd, &values);
	 smart_read_thresholds (fd, &thresholds);
	 retval = values_not_passed (&values, &thresholds);
	 break;
       default:
	 smart_read_values (fd, &values);
	 smart_read_thresholds (fd, &thresholds);
	 print_values (&values, &thresholds);
	 break;
      }      
   
      close (fd);
   }
   
   return retval;
}
