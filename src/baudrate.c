/* A tool to help quickly identify the baud rate of a given serial port. Displays serial port input while
 * allowing the user to change the serial port baud rate on the fly using the up/down arrow keys. This
 * is useful when attaching to unknown serial ports, such as those on embedded devices.
 * 
 * Note: This tool assumes serial port settings of 8 bits, no parity, 1 stop bit, and no handshaking.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <fcntl.h>
#include <termios.h>
#include "baudrate.h"

pthread_t pth;

int main(int argc, char *argv[])
{
	struct sigaction san, sao;
	int c = 0;

	/* Check usage */
	if(argc < MIN_ARGS){
		usage(argv[0]);
		goto end;
	}

	/* Initialize global configuration settings */
	memset((void *) &config, 0, sizeof(config));
        config.fd = -1;
        config.baud_index = DEFAULT_BAUD_RATES_INDEX;
	config.verbose = 1;
	config.prompt = 1;
	config.manual = 0;
	config.threaded = 0;
	config.threshold = DEFAULT_AUTO_THRESHOLD;
	config.wait_period = DEFAULT_WAIT_PERIOD;

	while((c = getopt(argc, argv, "bhmpqt:c:")) != -1){
		switch(c)
		{
			case 'q':
				config.verbose = 0;
				break;
			case 'p':
				config.prompt = 0;
				break;
			case 'm':
				config.manual = 1;
				break;
			case 't':
				config.wait_period = atoi(optarg);
				break;
			case 'c':
				config.threshold = atoi(optarg);
				break;
			case 'b':
				display_baud_rates();
				goto end;
			case 'h':
			default:
				usage(argv[0]);
				goto end;
		}
	}

	/* Get the serial port device name */
	config.port = strdup(argv[argc-1]);

	/* Open serial port */
	config.fd = open_serial_port();
        if(config.fd == -1){
                goto end;
        }

	/* Initialize sigaction structures */
        memset((void *) &san, 0, sizeof(san));
        memset((void *) &sao, 0, sizeof(sao));

	/* Set up SIGINT handler */
	san.sa_handler = sigint_handler;
	sigemptyset(&san.sa_mask);
	san.sa_flags = 0;
	sigaction(SIGINT, &san, &sao);

	/* SIGALRM handler, only used in auto mode */
	san.sa_handler = sigalrm_handler;
	sigemptyset(&san.sa_mask);
	san.sa_flags = 0;
	sigaction(SIGALRM, &san, &sao);

	/* Prevent defunct child processes */
	signal(SIGCHLD, SIG_IGN);

	/* Set initial serial device configuration */
	configure_serial_port();

	if(config.verbose){
		fprintf(stderr, "\nPress the up or down arrow keys to increase or decrease the baud rate.\n");
		fprintf(stderr, "Press Ctl+C to quit.\n");
	}

	/* Set the baud rate to DEFAULT_BAUD_RATE_INDEX */
	update_serial_baud_rate();

	/* Spawn a thread to read data from the serial port */
	if(pthread_create(&pth, NULL, read_serial, NULL) == 0)
	{
		config.threaded = 1;
		cli();
	}

end:
	/* We should never get here unless things go wrong, so always return exit failure */
	cleanup();
	return EXIT_FAILURE;
}

/* Open and configure serial port */
int open_serial_port()
{
	int fd = -1;

	/* Open serial device */
	fd = open(config.port, O_RDWR | O_NOCTTY | O_NDELAY);
	if(fd == -1){
		perror("Failed to open serial port");
	} else {
		fcntl(fd, F_SETFL, 0);
	}

	return fd;
}

/* Configure serial settings */
void configure_serial_port()
{
	struct termios termconfig = { 0 };

	/* Get existing serial port configuration settings */
        tcgetattr(config.fd, &termconfig);

	/* Save off existing settings */
	memcpy((void *) &config.termios, (void *) &termconfig, sizeof(struct termios));

        /* Enable reciever and set local mode */
        termconfig.c_cflag |= (CLOCAL | CREAD);

        /* Set the blocking time for subsequent read() calls */
        termconfig.c_cc[VMIN] = 0;
        termconfig.c_cc[VTIME] = READ_TIMEOUT;

        /* 8 bits, no parity, 1 stop bit */
        termconfig.c_cflag &= ~PARENB;
        termconfig.c_cflag &= ~CSTOPB;
        termconfig.c_cflag &= ~CSIZE;
        termconfig.c_cflag |= CS8;

        /* No hardware or software flow control */
        termconfig.c_cflag &= ~CRTSCTS;
        termconfig.c_iflag &= ~(IXON | IXOFF | IXANY);

        /* Raw input and output */
        termconfig.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
        termconfig.c_oflag &= ~OPOST;

        /* Apply changes NOW */
        tcsetattr(config.fd, TCSANOW, &termconfig);

	return;
}

/* Adjust serial port baud rate */
void update_serial_baud_rate()
{
	struct termios termconfig = { 0 };

	/* Ensure sane index values */
	if(config.baud_index < 0)
	{
		config.baud_index = DEFAULT_BAUD_RATES_INDEX;
	}
	else if(config.baud_index >= BAUD_RATES_SIZE)
	{
		config.baud_index = 0;
	}

        /* Get existing serial port configuration settings */
        tcgetattr(config.fd, &termconfig);

        /* Set input/output baud rate */
        cfsetispeed(&termconfig, BAUD_RATES[config.baud_index].baud);
        cfsetospeed(&termconfig, BAUD_RATES[config.baud_index].baud);

	/* Apply changes NOW */
        tcsetattr(config.fd, TCSANOW, &termconfig);

	if(config.verbose){	
		fprintf(stderr, "\n\n%s\n%sSerial baud rate set to: %s\n%s\n\n", DELIM, CENTER_PADDING, BAUD_RATES[config.baud_index].desc, DELIM);
	}
	return;
}

/* Provide simple command line interface */
void cli()
{
	char c = 0;
	struct termios tio = { 0 };

	/* Get stdin settings */
	tcgetattr(STDIN, &tio);

	/* Save off existing stdin settings */
        memcpy((void *) &config.stdinios, (void *) &tio, sizeof(struct termios));

	/* Put STDIN into raw mode */
	tio.c_lflag &= ~ICANON;
	tcsetattr(STDIN, TCSANOW, &tio);

	while(1){
		if(!config.manual)
		{
			sleep(10);
			continue;
		}

		c = getchar();

		/* Check to see if we got a valid UP or DOWN key value */	
		if(c == 'u' || c == 'U' || c == UP_ARROW){
			config.baud_index++;
		} else if (c == 'd' || c == 'D' || c == DOWN_ARROW){
			config.baud_index--;
		/* These are control characters that are part of the up/down arrow key presses */
		} else if (c == '\x1B' || c == '\x5B') {
			continue;
		}

		/* Erase any user-typed character(s) */		
		fprintf(stderr, "\b\b\b\b    \r");
	
		/* Update the serial port baud rate */
		update_serial_baud_rate();
	}

	return;
}

/* Infinite loop to read data from the serial port */
void *read_serial(void *arg)
{
	char buffer[1] = { 0 };
	int ascii = 0, punctuation = 0, whitespace = 0, vowels = 0;

	if(!config.manual)
	{
		alarm(config.wait_period);
	}
	
	while(1) 
	{
		memset((void *) &buffer, 0, 1);

		if(read(config.fd, &buffer, 1) == 1)
		{
			if(!config.manual)
			{	
				if((buffer[0] >= ' ' && buffer[0] <= '~') ||
				   (buffer[0] == '\n' || buffer[0] == '\r'))
				{
					ascii++;

					switch(buffer[0])
					{
						case ' ':
						case '\r':
						case '\n':
							whitespace++;
							break;
						case '.':
						case ',':
						case ';':
						case ':':
						case '!':
							punctuation++;
							break;
						case 'a':
						case 'e':
						case 'i':
						case 'o':
						case 'u':
							vowels++;
							break;
					}
				}
				else
				{
					ascii = 0;
					vowels = 0;
					whitespace = 0;
					punctuation = 0;
				}

				if(ascii >= config.threshold && whitespace && vowels && punctuation)
				{
					config.manual = 1;
					alarm(0);
					kill(getpid(), SIGINT);
					break;
				}
			}
			
			fprintf(stderr, "%c", buffer[0]);
			fflush(stderr);
		}
	}

	return NULL;
}

/* Prints the current serial port settings to stdout in a minicom compatible configuration format. */
void print_current_minicom_config()
{
	FILE *fs = NULL;
	char confile[FILENAME_MAX] = { 0 };
	char config_name[FILENAME_MAX] = { 0 };
	int copycount = FILENAME_MAX, config_name_size = 0;

	if(config.verbose && config.prompt){
		/* Prompt the user to save this configuration */
		fprintf(stderr, "\nSave serial port configuration as [none]: ");
		fgets((char *) &config_name, FILENAME_MAX-1, stdin);
		config_name_size = strlen((char *) &config_name);

		/* Remove the trailing new line */
		if(config_name_size > 0){
			config_name[config_name_size-1] = 0;
		}

		/* If a config name was entered, generate the minicom config file path and open it */
		if(config_name[0] != 0){
			/* Generate the full path to the minicom config file */
			strncpy(confile, MINICOM_CONFIG_DIR, copycount);
			copycount -= strlen(MINICOM_CONFIG_DIR);
			strncat(confile, MINICOM_CONFIG_PREFIX, copycount);
			copycount -= strlen(MINICOM_CONFIG_PREFIX);
			strncat(confile, (char *) &config_name, copycount);
			copycount -= config_name_size;

			/* Open the file for writing */
			fs = fopen(confile, "w");
			if(!fs){
				perror("Failed to open file for writing");
			}
		}
	}

	/* If no output file was specified, or if the file couldn't be opened, print to stdout */
	if(!fs){
		fs = stdout;
	}

	fprintf(stderr, "\n");

	/* Print minicom config data */
	fprintf(fs, "########################################################################\n");
	fprintf(fs, "# Minicom configuration file - use \"minicom -s\" to change parameters.\n");
	fprintf(fs, "pu port             %s\n", config.port);
	fprintf(fs, "pu baudrate         %s\n", BAUD_RATES[config.baud_index].desc);
	fprintf(fs, "pu bits             8\n");
	fprintf(fs, "pu parity           N\n");
	fprintf(fs, "pu stopbits         1\n");
	fprintf(fs, "pu rtscts           No\n");
	fprintf(fs, "########################################################################\n");

	if(fs != stdout){
		fclose(fs);
		fprintf(stderr, "\nMinicom configuration data saved to: %s\n", confile);
	}
	fprintf(stderr, "\n");

	return;
}

/* Clean up serial file descriptor and malloc'd data*/
void cleanup()
{
	if(config.threaded && pthread_cancel(pth) == 0)
	{
#ifdef __linux
		/* 
		 * In OSX, the thread cancellation is ignored until the thread's blocking getchar()
		 * function returns (i.e., the user presses a key. We're about to clean up and quit anyway,
		 * so just don't call pthread_join in OSX.
		 */
		void *thread_retval = NULL;
		pthread_join(pth, &thread_retval);
#endif
	}

	if(config.fd != -1) {
		/* Restore serial port settings */
		tcsetattr(config.fd, TCSANOW, &config.termios);

		/* Restore stdin settings */
		tcsetattr(STDIN, TCSANOW, &config.stdinios);

		/* Close serial port */
		close(config.fd);

		/* Print closing messages */
		if(config.verbose){
			fflush(stderr);
			fprintf(stderr, "\n\n%s\n%sDetected baud rate: %s baud\n%s\n\n", DELIM, CENTER_PADDING, BAUD_RATES[config.baud_index].desc, DELIM);
			fflush(stderr);
		}
		print_current_minicom_config();
	}

	/* Free pointers */
	if(config.port) free(config.port);
}

/* Handle Ctl+C */
void sigint_handler(int signum)
{
	cleanup();
	signum = 0;
	exit(EXIT_SUCCESS);
}

/* Handle sigalrm actions (needed for auto mode) */
void sigalrm_handler(int signum)
{
	signum = 0;

	if(!config.manual)
	{
		config.baud_index--;
		update_serial_baud_rate();
		alarm(config.wait_period);
	}
}

/* Displays the supported baud rates (see baudrate.h for enabling additoinal baud rates) */
void display_baud_rates()
{
	int i = 0;

        fprintf(stderr, "\n");

        for(i=0; i < BAUD_RATES_SIZE; i++){
                fprintf(stderr, "%6s baud\n", BAUD_RATES[i].desc);
        }

	fprintf(stderr, "\n");

	return;
}

void usage(char *prog_name)
{
	fprintf(stderr, "\n");
	fprintf(stderr, "Baudrate v%s\n", VERSION);
	fprintf(stderr, "\n");
	fprintf(stderr, "Usage: %s [OPTIONS] [DEVICE]\n", prog_name);
	fprintf(stderr, "\n");
	fprintf(stderr, "\t-b   Display supported baud rates\n");
	fprintf(stderr, "\t-p   Disable interactive prompts\n");
	fprintf(stderr, "\t-q   Enable quiet mode (implies -p)\n");
	fprintf(stderr, "\t-h   Display help\n");
	fprintf(stderr, "\n");

	return;
}
