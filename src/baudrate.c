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
#include <fcntl.h>
#include <termios.h>
#include "baudrate.h"

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
        config.ppid = getpid();
        config.fd = -1;
        config.baud_index = DEFAULT_BAUD_RATES_INDEX;
	config.verbose = 1;
	config.prompt = 1;

	while((c = getopt(argc, argv, "bhpq")) != -1){
		switch(c)
		{
			case 'q':
				config.verbose = 0;
				break;
			case 'p':
				config.prompt = 0;
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

	/* Spawn a child process to read data from the serial port */
	if(!fork()){
		read_serial();
	} else {
	/* While the parent handles user commands */
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
	struct termios tio;

	/* Put STDIN into raw mode */
	tcgetattr(STDIN, &tio);
	tio.c_lflag &= ~ICANON;
	tcsetattr(STDIN, TCSANOW, &tio);

	while(1){
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
	
		/* Ensure sane index values */	
		if(config.baud_index < 0){
			config.baud_index = 0;
		} else if(config.baud_index >= BAUD_RATES_SIZE){
			config.baud_index = BAUD_RATES_SIZE-1;
		}

		/* Update the serial port baud rate */
		update_serial_baud_rate();
	}

	return;
}

/* Infinite loop to read data from the serial port */
void read_serial()
{
	char buffer[1] = { 0 };
	
	while(1) {
		memset((void *) &buffer, 0, 1);

		if(read(config.fd, &buffer, 1) == 1){
			fprintf(stderr, "%c", buffer[0]);
		}
	}

	return;
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
	/* Only the parent process is allowed to do this */
	if(getpid() == config.ppid){
		
		if(config.fd != -1) {
			/* Restore serial port settings */
			tcsetattr(config.fd, TCSANOW, &config.termios);

			/* Close serial port */
			close(config.fd);

			/* Print closing messages */
			if(config.verbose){
				fprintf(stderr, "\n\n%s\n%sEnding baud rate: %s baud\n%s\n\n", DELIM, CENTER_PADDING, BAUD_RATES[config.baud_index].desc, DELIM);
			}
			print_current_minicom_config();
		}

		/* Free pointers */
		if(config.port) free(config.port);
	}
}

/* Handle Ctl+C */
void sigint_handler(int signum)
{
	cleanup();
	signum = 0;
	exit(EXIT_SUCCESS);
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
