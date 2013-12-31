#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#define PIDFILE "/tmp/timer.pid"

//Options when calling the program.
enum mode { 
	BEGIN,
	PAUSE,
	RESTART,
	STOP
}; 

struct itimerval timer;
FILE *pidfile;
char *timerfilename = "/tmp/.timer";
int istimercreated = 0;
struct tm tms;

void
alarmhandler(int sig)
{
	int err;
	char timestr[9];
	FILE *timerfile;

	tms.tm_sec += 1;
	if (tms.tm_sec == 60) {
		tms.tm_min += 1;
		tms.tm_sec -= 60;
	}
	if (tms.tm_min == 60) {
		tms.tm_hour += 1;
		tms.tm_min -= 60;
	}
	if (tms.tm_hour == 24)
		tms.tm_hour = 0;

	snprintf(timestr, sizeof(timestr), "%.2d:%.2d:%.2d", tms.tm_hour, tms.tm_min, tms.tm_sec);

	if ((timerfile = fopen(timerfilename, "w+")) == NULL) {
		printf("Couldn't open %s for write it\n", timerfilename);
		exit(1);
	}
	fprintf(timerfile, "%s", timestr);
	if (fclose(timerfile) != 0)
		printf("Error: Couldn't close the timer file\n");
	istimercreated = 1;
}

void
usr1handler(int sig)
{
	int err;

	if (timer.it_value.tv_sec == 0)
		timer.it_value.tv_sec = 1;
	else if (timer.it_value.tv_sec == 1)
		timer.it_value.tv_sec = 0;

	if ((err = setitimer(ITIMER_REAL, &timer, NULL)) != 0) {
		perror("Couldn't set the timer:");
		exit(1);
	}
}

void
usr2handler(int sig)
{
	tms.tm_sec = tms.tm_min = tms.tm_hour = 0;
}

void
termhandler(int sig)
{
	int err;
	if ((err = remove(PIDFILE)) != 0)
		printf("Couldn't delete the file %s, please delete it manually.\n", PIDFILE);
	if (istimercreated)
		if ((err = remove(timerfilename)) != 0)
			printf("Couldn't delete the file %s, please delete it manually.\n", timerfilename);
	exit(0);
}

void
usage(char *name)
{
	printf("Usage: %s [ [p]ause | [r]estart | [s]top ]\n", name);
	exit(1);
}

int
begin()
{
	FILE *timerfile;
	int err;
	int pid;

	pid = (int)getpid();
	putw(pid, pidfile);
	fclose(pidfile);
	
	signal(SIGALRM, alarmhandler);
	signal(SIGUSR1, usr1handler);
	signal(SIGUSR2, usr2handler);
	signal(SIGTERM, termhandler);
	
	timer.it_interval.tv_sec = 1;
	timer.it_interval.tv_usec = 0;
	timer.it_value.tv_sec = 1;
	timer.it_value.tv_usec = 0;

	if ((err = setitimer(ITIMER_REAL, &timer, NULL)) != 0) {
		perror("Couldn't set the timer:");
		exit(1);
	}
		
	while (1) {
		//Just iterate and let the time pass.
		pause();
	}
	return(0);
}

int
pausetimer(pid_t pid)
{
	int err;

	if ((err = kill(pid, SIGUSR1)) != 0) {
		printf("Error: Couldn't pause the timer.\n");
	}
	return(0);
}

int
restarttimer(pid_t pid)
{
	int err;

	if ((err = kill(pid, SIGUSR2)) != 0) {
		printf("Error: Couldn't pause the timer.\n");
	}
	return(0);
}

int
stoptimer(pid_t pid)
{
	int err;

	if ((err = kill(pid, SIGTERM)) != 0) {
		printf("Error: Couldn't send the SIGTERM signal to the timer.\n");
		printf("Please kill the timer process manually\n");
	}
	return(0);
}

int
main(int argc, char *argv[])
{
	char *home;
	int err;
	enum mode m;
	pid_t timerpid;
	struct stat st;

	if (argc == 2)
		switch (argv[1][0]) { 
		case 'p':
			m = PAUSE;
			break;
		case 'r':
			m = RESTART;
			break;
		case 's':
			m = STOP;
			break;
		default:
			usage(argv[0]);
			break;
		}
	else if (argc == 1)
		m = BEGIN;
	else
		usage(argv[0]);		

	//Check if we're starting the timer
	//and the file exists.
	err = access(PIDFILE, F_OK);
	if (m !=  BEGIN && err != 0) {
		printf("Timer is not running, please start it first.\n");
		exit(1);
	} else if (m == BEGIN && err == 0) {
		printf("Timer is already running, please stop it first.\n");
		exit(1);
	}

	//Create or read the file used to store the PID of the timer.
	if (m != BEGIN)
		pidfile = fopen(PIDFILE, "r+");
	else if (m == BEGIN)
		pidfile = fopen(PIDFILE, "w+");

	if (pidfile == NULL && m != BEGIN) {
		printf("Couldn't open %s for read it\n", PIDFILE);
		exit(1);
	} else if (pidfile == NULL && m == BEGIN) {
		printf("Couldn't create %s.\n", PIDFILE);
		exit(1);
	}

	if (m != BEGIN)
		timerpid = (pid_t)getw(pidfile);

	if ((home = getenv("HOME")) == NULL) {
		printf("Error: No $HOME, you hobbo!.\n");
		printf("Continuing anyways.\n");
	} else
		timerfilename = strncat(home, "/.timer", sizeof(timerfilename));

	switch (m) {
	case BEGIN:
		begin();
		break;
	case PAUSE:
		pausetimer(timerpid);
		break;
	case RESTART:
		restarttimer(timerpid);
		break;
	case STOP:
		stoptimer(timerpid);
		break;
	}	

	exit (0);
}
