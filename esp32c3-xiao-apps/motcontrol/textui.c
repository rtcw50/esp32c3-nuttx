#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>

#define MAX_LINES 50
#define LINE_WIDTH 128
#define PROMPT "mc> "

static int FD;

extern int g_ramp_time;
extern int g_speed;
extern int g_duration;
extern int g_agitate_duration;
extern int g_mode;

struct ui_ctx {
	struct termios orig_termios;
	int win_rows;
	int win_cols;
	char lines[MAX_LINES][LINE_WIDTH];
	int line_count;
	char status[LINE_WIDTH];
	int quit;
};

static struct ui_ctx g_ui_ctx;

static void disable_raw_mode(struct ui_ctx *ctx)
{
	tcsetattr(FD, TCSAFLUSH, &ctx->orig_termios);
	printf("\x1b[?25h"); // show cursor
	printf("\x1b[0m");
	printf("\x1b[H\x1b[2J");
	fflush(stdout);
}

static int enable_raw_mode(struct ui_ctx *ctx)
{
	if (tcgetattr(FD, &ctx->orig_termios) == -1) return -1;

	struct termios raw = ctx->orig_termios;
	raw.c_lflag &= ~(ECHO | ICANON);
	raw.c_lflag &= ~(ISIG);
	raw.c_iflag &= ~(IXON | ICRNL);
	raw.c_oflag &= ~(OPOST);
	raw.c_cc[VMIN] = 1;
	raw.c_cc[VTIME] = 0;

	if (tcsetattr(FD, TCSAFLUSH, &raw) == -1) return -1;
	return 0;
}

static void get_window_size(struct ui_ctx *ctx)
{
	struct winsize ws;
	if (ioctl(FD, TIOCGWINSZ, &ws) == -1) {
		ctx->win_rows = 24;
		ctx->win_cols = 80;
	} else {
		ctx->win_rows = ws.ws_row;
		ctx->win_cols = ws.ws_col;
	}
}

static void append_line(struct ui_ctx *ctx, const char *s)
{
	if (ctx->line_count < MAX_LINES) {
		strncpy(ctx->lines[ctx->line_count], s, LINE_WIDTH-1);
		ctx->lines[ctx->line_count][LINE_WIDTH-1] = '\0';
		ctx->line_count++;
	} else {
		memmove(ctx->lines, ctx->lines+1, (MAX_LINES-1)*LINE_WIDTH);
		strncpy(ctx->lines[MAX_LINES-1], s, LINE_WIDTH-1);
		ctx->lines[MAX_LINES-1][LINE_WIDTH-1] = '\0';
	}
}

static void draw_ui(struct ui_ctx *ctx, const char *input, int cursor_pos)
{
	get_window_size(ctx);
	int output_rows = ctx->win_rows - 2;

	printf("\x1b[H\x1b[2J");

	int start = ctx->line_count - output_rows;
	if (start < 0) start = 0;
	for (int i = start; i < ctx->line_count; ++i) {
		int n = (ctx->win_cols < LINE_WIDTH) ? ctx->win_cols : LINE_WIDTH;
		char buf[LINE_WIDTH+1];
		strncpy(buf, ctx->lines[i], n);
		buf[n] = '\0';
		printf("%s\r\n", buf);
	}

	int printed = ctx->line_count - start;
	for (int i = printed; i < output_rows; ++i) printf("\r\n");

	printf("%s", PROMPT);
	int avail = ctx->win_cols - (int)strlen(PROMPT);
	if (avail < 0) avail = 0;
	if (avail > LINE_WIDTH) avail = LINE_WIDTH;
	char tmp[LINE_WIDTH+1];
	strncpy(tmp, input, avail);
	tmp[avail] = '\0';
	printf("%s", tmp);
	printf("\x1b[K\r\n");

	printf("\x1b[7m");
	char timestr[64];
	char stbuf[sizeof(timestr) + sizeof(ctx->status) + sizeof(" | ")];
	int mins = g_duration / 60;
	int secs = g_duration % 60;
	snprintf(timestr, sizeof(timestr), "%02d:%02d", mins, secs);	
	snprintf(stbuf, sizeof(stbuf), " %s | %s", timestr, ctx->status);
	int status_trunc = (ctx->win_cols < LINE_WIDTH) ? ctx->win_cols : LINE_WIDTH;
	stbuf[status_trunc] = '\0';
	printf("%-*s", ctx->win_cols, stbuf);
	printf("\x1b[0m\r\n");

	int cursor_col = (int)strlen(PROMPT) + cursor_pos + 1;
	if (cursor_col > ctx->win_cols) cursor_col = ctx->win_cols;
	printf("\x1b[%d;%dH", output_rows + 1, cursor_col);
	printf("\x1b[?25l");
	fflush(stdout);
	printf("\x1b[?25h");
}

static void handle_command(struct ui_ctx *ctx, char *cmd)
{
	while (*cmd == ' ') cmd++;
	if (*cmd == '\0') {
		snprintf(ctx->status, sizeof(ctx->status), "Ready");
		return;
	}
	char *word = strtok(cmd, " ");
	if (!word) return;

	if (strcmp(word, "help") == 0) {
		append_line(ctx, "Commands:");
		append_line(ctx, "  help                       Show this help");
		append_line(ctx, "  duration <secs>            Set total run time in seconds");
		append_line(ctx, "  speed <rpm>                Set speed in RPM");
		append_line(ctx, "  ramp_time <secs>           Set ramp up/down time in seconds");
		append_line(ctx, "  agitate_duration <secs>    Set agitation time in seconds");
		append_line(ctx, "  run                        Run the motor");
		append_line(ctx, "  stop                       Stop the motor");
		append_line(ctx, "  pause                      Pause the motor");
		append_line(ctx, "  resume                     Resume the motor");
		append_line(ctx, "  show                       Show the settings");
		append_line(ctx, "  quit(or ctrl-c)            Exit the UI");
		snprintf(ctx->status, sizeof(ctx->status), "Displayed help");
	} else if (strcmp(word, "duration") == 0) {
		char *arg = strtok(NULL, " ");
		if (arg) {
			g_duration = atoi(arg);
		} else {
			append_line(ctx, "Usage: duration <seconds>");
			snprintf(ctx->status, sizeof(ctx->status), "Missing argument");
		}
	} else if (strcmp(word, "speed") == 0) {
		char *arg = strtok(NULL, " ");
		if (arg) {
			g_speed = atoi(arg);
		} else {
			append_line(ctx, "Usage: speed <rpm>");
			snprintf(ctx->status, sizeof(ctx->status), "Missing argument");
		}
	} else if (strcmp(word, "ramp_time") == 0) {
		char *arg = strtok(NULL, " ");
		if (arg) {
			g_ramp_time = atoi(arg);
		} else {
			append_line(ctx, "Usage: ramp_time <seconds>");
			snprintf(ctx->status, sizeof(ctx->status), "Missing argument");
		}
	} else if (strcmp(word, "agitate_duration") == 0) {
		char *arg = strtok(NULL, " ");
		if (arg) {
			g_agitate_duration = atoi(arg);
		} else {
			append_line(ctx, "Usage: agitate_duration <seconds>");
			snprintf(ctx->status, sizeof(ctx->status), "Missing argument");
		}
	} else if (strcmp(word, "run") == 0) {
		g_mode = 1;
		snprintf(ctx->status, sizeof(ctx->status), "Motor running");
	} else if (strcmp(word, "stop") == 0) {
		g_mode = 0;
		snprintf(ctx->status, sizeof(ctx->status), "Motor stopped");
	} else if (strcmp(word, "pause") == 0) {
		g_mode = 2;
		snprintf(ctx->status, sizeof(ctx->status), "Motor paused");
	} else if (strcmp(word, "resume") == 0) {
		g_mode = 1;
		snprintf(ctx->status, sizeof(ctx->status), "Motor running");
	} else if (strcmp(word, "show") == 0) {
		char buf[LINE_WIDTH];
		snprintf(buf, sizeof(buf), "Settings:");
		append_line(ctx, buf);
		snprintf(buf, sizeof(buf), " duration=%ds", g_duration);
		append_line(ctx, buf);
		snprintf(buf, sizeof(buf), " speed=%drpm", g_speed);
		append_line(ctx, buf);
		snprintf(buf, sizeof(buf), " ramp_time=%ds", g_ramp_time);
		append_line(ctx, buf);
		snprintf(buf, sizeof(buf), " agitate_duration=%ds", g_agitate_duration);
		append_line(ctx, buf);
	} else if (strcmp(word, "quit") == 0 || strcmp(word, "exit") == 0) {
		ctx->quit = 1;
	} else {
		snprintf(ctx->status, sizeof(ctx->status), "Unknown command");
	}
}

int textui_loop(int fd)
{
	FD = fd;
//	struct ui_ctx *ctx = calloc(1, sizeof(struct ui_ctx));
	struct ui_ctx *ctx = &g_ui_ctx;
	if (!ctx) return -1;

	if (enable_raw_mode(ctx) == -1) {
		//free(ctx);
		return -1;
	}
	get_window_size(ctx);

	snprintf(ctx->status, sizeof(ctx->status), "Ready");
	append_line(ctx, "Welcome to the persistent text UI (no ncurses).");
	append_line(ctx, "Type 'help' for available commands.");

	char input[LINE_WIDTH];
	int ipos = 0;
	input[0] = '\0';

	while (!ctx->quit) {
		draw_ui(ctx, input, ipos);

		char c = 0;
		ssize_t n = read(FD, &c, 1);
		if (n <= 0) continue;

		if (c == '\r' || c == '\n') {
			input[ipos] = '\0';
			char cmd[LINE_WIDTH];
			strncpy(cmd, input, LINE_WIDTH-1);
			cmd[LINE_WIDTH-1] = '\0';
			append_line(ctx, "");
			if (strlen(cmd) > 0) {
				char echo_line[LINE_WIDTH + sizeof(PROMPT)];
				snprintf(echo_line, sizeof(echo_line), "%s%s", PROMPT, cmd);
				append_line(ctx, echo_line);
			}
			handle_command(ctx, cmd);
			ipos = 0;
			input[0] = '\0';
		} else if (c == 127 || c == 8) {
			if (ipos > 0) {
				ipos--;
				input[ipos] = '\0';
			}
		} else if (c == 27) {
			char seq[3];
			if (read(FD, &seq[0], 1) == 0) continue;
			if (read(FD, &seq[1], 1) == 0) continue;
		} else if (c >= 32 && c <= 126) {
			if (ipos < LINE_WIDTH-1) {
				input[ipos++] = c;
				input[ipos] = '\0';
			}
		}
		else if (c == 3) {
			ctx->quit = 1;
		}
	}

	disable_raw_mode(ctx);
	//free(ctx);
	return 0;
}
