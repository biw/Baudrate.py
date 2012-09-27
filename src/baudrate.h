#define VERSION				"0.3"
#define MIN_ARGS			2
#define READ_TIMEOUT			100
#define DEFAULT_AUTO_THRESHOLD		25
#define DEFAULT_WAIT_PERIOD		5
#define STDIN				0
#define DELIM				"@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@"
#define CENTER_PADDING			"                  "

#define MINICOM_CONFIG_DIR		"/etc/minicom/"
#define MINICOM_CONFIG_PREFIX		"minirc."
#ifndef MINICOM_BIN_PATH
#define MINICOM_BIN_PATH		"/usr/bin/minicom"
#endif

/* The up and down arrows are actually comprised of three characters:

	Up:   \x1B\x5B\x41
	Down: \x1B\x5B\x42

   Since the only byte that is different between them is the last, we
   just look for the last character to determine an up or down arrow key press.
*/
#define UP_ARROW			'A'
#define DOWN_ARROW			'B'

struct globals
{
	/* Serial file descriptor */
	int fd;
	/* Current index into the baud_rates table */
	int baud_index;
	/* Controls verbose mode */
	int verbose;
	/* Controls if the user gets prompted for a config file name or not */
	int prompt;
	/* Set to 0 for auto mode. */
	int manual;
	/* Only set if the reader thread is successfully started. */
	int threaded;
	/* The number of consecutive ASCII bytes to wait for while in auto mode */
	int threshold;
	/* Seconds to wait before cycling to the next baudrate while in auto mode */
	int wait_period;
	/* Incremented each time the SIGALRM handler is called while in auto mode */
	int timeout_count;
	/* If set to 1, minicom will be invoked when the config file is saved */
	int minicom;
	/* Points to the minicom config file */
	char *out_name;
	/* Serial port name */
	char *port;
	/* Holds serial port configuration settings so that they can be restored */
	struct termios termios;
	/* Holds stdin configuration settings so that they can be restored */
	struct termios stdinios;
} config;

/* Each baud rate entry must have a machine readable (int) and human readable (char *) baud rate */
struct baud_rate_entry
{
	int baud;
	char *desc;
};

/* Only include the most common baud rates to minimize the number of guesses needed to find the right one.
 * To add support for additional baud rates, uncomment them here and re-compile.
 */
struct baud_rate_entry BAUD_RATES[] = {
//	{ B50, "50" },
//	{ B75, "75" },
//	{ B110, "110" },
//	{ B134, "134" },
//	{ B150, "150" },
//	{ B200, "200" },
//	{ B300, "300" },
//	{ B600, "600" },
	{ B1200, "1200" },
	{ B1800, "1800" },
	{ B2400, "2400" },
	{ B4800, "4800" },
	{ B9600, "9600" },
	{ B19200, "19200" },
	{ B38400, "38400" },
	{ B57600, "57600" },
	{ B115200, "115200" }
//	{ B230400, "230400" },
//	{ B460800, "460800" },
//	{ B500000, "500000" },
//	{ B576000, "576000" },
//	{ B921600, "921600" },
//	{ B1000000, "1000000" },
//	{ B1152000, "1152000" },
//	{ B1500000, "1500000" },
//	{ B2000000, "2000000" },
//	{ B2500000, "2500000" },
//	{ B3000000, "3000000" },
//	{ B3500000, "3500000" },
//	{ B4000000, "4000000" },
};

#define BAUD_RATES_SIZE			(sizeof(BAUD_RATES)/sizeof(struct baud_rate_entry))
#define DEFAULT_BAUD_RATES_INDEX	BAUD_RATES_SIZE-1

int open_serial_port(void);
void configure_serial_port(void);
void update_serial_baud_rate(void);
void cli(void);
void *read_serial(void *arg);
void print_current_minicom_config(void);
void cleanup(void);
void sigint_handler(int signum);
void sigalrm_handler(int signum);
void display_baud_rates(void);
void usage(char *prog_name);
