#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <mqueue.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include "motcontrol.h"

#define MAX_LINES 50
#define LINE_WIDTH 80 
#define TIME_REMAINING_WIDTH 20 
#define STATUS_WIDTH 30 
#define PROMPT "mc> "

static int FD;

static struct clean_cmd_msg_s g_cmd_msg;
static mqd_t cmd_q;
static mqd_t tel_q;

static int ui_dirty_top = 1;
static int ui_dirty_bottom = 1;

extern int g_ramp_time;
extern int g_speed;
extern int g_duration;
extern int g_agitate_duration;

static char *g_motor_messages[] = {
    "Ready",
};


struct ui_ctx {
    struct termios orig_termios;
    int win_rows;
    int win_cols;
    char lines[MAX_LINES][LINE_WIDTH];
    int line_count;
    char str_time_remaining[TIME_REMAINING_WIDTH];
    char str_status[STATUS_WIDTH];
    int quit;
};

struct input_ctx {
    char input[LINE_WIDTH];
    int ipos;
};

static struct ui_ctx g_ui_ctx;

static void disable_raw_mode(struct ui_ctx *ctx)
{
    tcsetattr(FD, TCSAFLUSH, &ctx->orig_termios);
    printf("\x1b[?25h"); // show cursor
    write(FD, "\x1b[?25h", 6);                         //
    printf("\x1b[0m");
    printf("\x1b[H\x1b[2J");
    fflush(stdout);
}

static int enable_raw_mode(struct ui_ctx *ctx)
{
    if (tcgetattr(FD, &ctx->orig_termios) == -1) return -1;

    struct termios raw = ctx->orig_termios;
    raw.c_lflag &= ~(ECHO | ECHOE | ICANON | ISIG);
    raw.c_iflag &= ~(IXON | IXOFF | IXANY |ICRNL);
    raw.c_oflag &= ~(OPOST);
    raw.c_cc[VMIN] = 0;  // was 1
    raw.c_cc[VTIME] = 0;

    if (tcsetattr(FD, TCSANOW, &raw) == -1) return -1;
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
    ui_dirty_top = 1;
}
static void clear_ui(struct ui_ctx *ctx)
{
    for (int i = 0; i < MAX_LINES; i++) {
        ctx->lines[i][0] = '\0';
    }
    ctx->line_count = 0;
    ui_dirty_top = 1;
    ui_dirty_bottom = 1;
}
static void draw_history_pane(struct ui_ctx *ctx)
{
    get_window_size(ctx);
    int output_rows = ctx->win_rows - 2;

    char frame[8192];
    size_t off = 0;
    size_t cap = sizeof(frame);

    int start = ctx->line_count - output_rows;
    if (start < 0) start = 0;

    for (int row = 0; row < output_rows; ++row) {
        int line_idx = start + row;

        off += snprintf(frame + off, (off < cap) ? cap - off : 0,
                        "\x1b[%d;1H\x1b[2K", row + 1);

        if (line_idx < ctx->line_count) {
            int n = (ctx->win_cols < LINE_WIDTH) ? ctx->win_cols : LINE_WIDTH;
            char buf[LINE_WIDTH + 1];
            strncpy(buf, ctx->lines[line_idx], n);
            buf[n] = '\0';
            off += snprintf(frame + off, (off < cap) ? cap - off : 0, "%s", buf);
        }

        if (off >= cap - 128) break;
    }

    if (off > 0) write(FD, frame, off);
}

static void draw_prompt_status_pane(struct ui_ctx *ctx, const struct input_ctx *input_ctx)
{
    get_window_size(ctx);

    int content_rows = ctx->win_rows - 2;  // static display area
    int prompt_row = content_rows + 1;       // second-to-last row
    int status_row = content_rows + 2;      // last row

    char frame[4096];
    size_t off = 0;
    size_t cap = sizeof(frame);

    // Disable auto-wrap
    off += snprintf(frame + off, (off < cap) ? cap - off : 0,
                    "\x1b[?7l");
    // Clear the bottom two rows first
    off += snprintf(frame + off, (off < cap) ? cap - off : 0,
                    "\x1b[%d;1H\x1b[2K", prompt_row);
    off += snprintf(frame + off, (off < cap) ? cap - off : 0,
                    "\x1b[%d;1H\x1b[2K", status_row);

    // Prompt line
    off += snprintf(frame + off, (off < cap) ? cap - off : 0,
                    "\x1b[%d;1H", prompt_row);
    off += snprintf(frame + off, (off < cap) ? cap - off : 0, "%s", PROMPT);

    int avail = ctx->win_cols - (int)strlen(PROMPT);
    if (avail < 0) avail = 0;
    if (avail > LINE_WIDTH) avail = LINE_WIDTH;

    char tmp[LINE_WIDTH + 1];
    strncpy(tmp, input_ctx->input, avail);
    tmp[avail] = '\0';
    off += snprintf(frame + off, (off < cap) ? cap - off : 0, "%s", tmp);

    #if 1
    // Status line
    off += snprintf(frame + off, (off < cap) ? cap - off : 0,
                    "\x1b[%d;1H", status_row);
    off += snprintf(frame + off, (off < cap) ? cap - off : 0, "\x1b[7m");

    char stbuf[sizeof(ctx->str_time_remaining) + sizeof(ctx->str_status) + sizeof(" | ")];
    snprintf(stbuf, sizeof(stbuf), " %s | %s", ctx->str_time_remaining, ctx->str_status);
    int status_trunc = (ctx->win_cols < LINE_WIDTH) ? ctx->win_cols : LINE_WIDTH;
    stbuf[status_trunc] = '\0';
    off += snprintf(frame + off, (off < cap) ? cap - off : 0, "%-*s", ctx->win_cols, stbuf);
    off += snprintf(frame + off, (off < cap) ? cap - off : 0, "\x1b[0m");
    #endif

    // Put cursor back on the prompt line
    int cursor_col = (int)strlen(PROMPT) + input_ctx->ipos + 1;
    if (cursor_col > ctx->win_cols) cursor_col = ctx->win_cols;
    off += snprintf(frame + off, (off < cap) ? cap - off : 0,
                    "\x1b[?7l\x1b[%d;%dH", prompt_row, cursor_col);

    if (off > 0) {
        size_t sent = 0;
        while (sent < off) {
            ssize_t n = write(FD, frame + sent, off - sent);
            if (n <= 0) break;
            sent += n;
        }
        tcdrain(FD);
    }
}

static void handle_command(struct ui_ctx *ctx, char *cmd)
{
    while (*cmd == ' ') cmd++;
    if (*cmd == '\0') {
        snprintf(ctx->str_status, sizeof(ctx->str_status), "Ready");
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
        snprintf(ctx->str_status, sizeof(ctx->str_status), "Displayed help");
    } else if (strcmp(word, "duration") == 0) {
        char *arg = strtok(NULL, " ");
        if (arg) {
            g_duration = atoi(arg);
        } else {
            append_line(ctx, "Usage: duration <seconds>");
            snprintf(ctx->str_status, sizeof(ctx->str_status), "Missing argument");
        }
    } else if (strcmp(word, "speed") == 0) {
        char *arg = strtok(NULL, " ");
        if (arg) {
            g_speed = atoi(arg);
        } else {
            append_line(ctx, "Usage: speed <rpm>");
            snprintf(ctx->str_status, sizeof(ctx->str_status), "Missing argument");
        }
    } else if (strcmp(word, "ramp_time") == 0) {
        char *arg = strtok(NULL, " ");
        if (arg) {
            g_ramp_time = atoi(arg);
        } else {
            append_line(ctx, "Usage: ramp_time <seconds>");
            snprintf(ctx->str_status, sizeof(ctx->str_status), "Missing argument");
        }
    } else if (strcmp(word, "agitate_duration") == 0) {
        char *arg = strtok(NULL, " ");
        if (arg) {
            g_agitate_duration = atoi(arg);
        } else {
            append_line(ctx, "Usage: agitate_duration <seconds>");
            snprintf(ctx->str_status, sizeof(ctx->str_status), "Missing argument");
        }
    } else if (strcmp(word, "run") == 0) {
        snprintf(ctx->str_status, sizeof(ctx->str_status), "Sent run command");
        g_cmd_msg.command = MSG_RUN;
        g_cmd_msg.run_time_s = g_duration;
        g_cmd_msg.max_duty = 50;
        mq_send(cmd_q, (const char *)&g_cmd_msg, sizeof(g_cmd_msg), 0);
    } else if (strcmp(word, "stop") == 0) {
        snprintf(ctx->str_status, sizeof(ctx->str_status), "Sent stop command");
        g_cmd_msg.command = MSG_STOP_REQ;
        mq_send(cmd_q, (const char *)&g_cmd_msg, sizeof(g_cmd_msg), 0);
    } else if (strcmp(word, "pause") == 0) {
        g_cmd_msg.command = MSG_PAUSE;
        snprintf(ctx->str_status, sizeof(ctx->str_status), "Sent pause command");
        mq_send(cmd_q, (const char *)&g_cmd_msg, sizeof(g_cmd_msg), 0);
    } else if (strcmp(word, "resume") == 0) {
        g_cmd_msg.command = MSG_RESUME;
        snprintf(ctx->str_status, sizeof(ctx->str_status), "Sent resume command");
        mq_send(cmd_q, (const char *)&g_cmd_msg, sizeof(g_cmd_msg), 0);
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
        snprintf(ctx->str_status, sizeof(ctx->str_status), "Unknown command");
    }
}

static int get_io_fd(void)
{
    // Return the file descriptor for the input/output
    int fd = -1;
    int nopen_tries = 50;
    usleep(300000); /* wait for ACM0 to be ready */
    while ((fd < 0) && nopen_tries > 0) {
        fd = open("/dev/ttyACM0", O_RDWR|O_NOCTTY);
        if (fd < 0) {
            usleep(100000); /* wait and try again */
        }
        nopen_tries--;
    }
    if (fd < 0) {
        printf("Failed to open /dev/ttyACM0\n");
        return -1;
    }
    usleep(500000); /* wait for USB handshake to complete */ 

    // Duplicate raw ACM0 descriptor to stdin, stdout, stderr
    dup2(fd, STDIN_FILENO);
    dup2(fd, STDOUT_FILENO);
    dup2(fd, STDERR_FILENO);
    return fd;
}

static void input_handler(struct ui_ctx *ctx, struct input_ctx *input_ctx, char c)
{
    ctx->quit = 0;

    if (c == '\r' || c == '\n') {
        input_ctx->input[input_ctx->ipos] = '\0';
        char cmd[LINE_WIDTH];
        strncpy(cmd, input_ctx->input, LINE_WIDTH-1);
        cmd[LINE_WIDTH-1] = '\0';

        append_line(ctx, "");
        if (strlen(cmd) > 0) {
            char echo_line[LINE_WIDTH + sizeof(PROMPT)];
            snprintf(echo_line, sizeof(echo_line), "%s%s", PROMPT, cmd);
            append_line(ctx, echo_line);
        }

        handle_command(ctx, cmd);

        input_ctx->ipos = 0;
        input_ctx->input[0] = '\0';
        ui_dirty_bottom = 1;
    } else if (c == 127 || c == 8) {
        if (input_ctx->ipos > 0) {
            input_ctx->ipos--;
            input_ctx->input[input_ctx->ipos] = '\0';
        }
        ui_dirty_bottom = 1;
    } else if (c == 27) {
        ;
    } else if (c >= 32 && c <= 126) {
        if (input_ctx->ipos < LINE_WIDTH-1) {
            input_ctx->input[input_ctx->ipos++] = c;
            input_ctx->input[input_ctx->ipos] = '\0';
        }
        ui_dirty_bottom = 1;
    } else if (c == 3) {
        ctx->quit = 1;
    }
}

int textui_loop(int argc, char *argv[])
{
    /* Get the file descriptor for the input/output */
    FD = get_io_fd();
    if (FD < 0) return -1;
    setvbuf(stdout, NULL, _IONBF, 0);
    write(FD, "\x1b[?25l", 6);

    struct ui_ctx *ctx = &g_ui_ctx;
    ctx->quit = 0;

    /* Set up the input/output queues */
    /* Open the TX channel to the motor (Write Only) */
    cmd_q = mq_open("/cleaner_cmd_q", O_WRONLY);
    
    /* Open the RX channel from the motor (Read Only) */
    /* Note: We keep this blocking, so the UI task sleeps until the motor sends data */
    tel_q = mq_open("/cleaner_tel_q", O_RDONLY);

    if (enable_raw_mode(ctx) == -1) {
        return -1;
    }
    get_window_size(ctx);

    snprintf(ctx->str_status, sizeof(ctx->str_status), "Ready");
    snprintf(ctx->str_time_remaining, sizeof(ctx->str_time_remaining), "%02d:%02d", g_duration / 60, g_duration % 60);

    struct input_ctx input_ctx = {
        .input = {0},
        .ipos = 0
    };
    
    // We'll poll the input file descriptor for input and read it if available
    struct pollfd pfd = {
        .fd = FD,
        .events = POLLIN
    };
    
    clear_ui(ctx);
    struct timespec last_draw;
    clock_gettime(CLOCK_MONOTONIC, &last_draw);
    char c = 0;
    while (!ctx->quit) {
        if (ui_dirty_top) {
            draw_history_pane(ctx);
            ui_dirty_top = 0;
        }

        if (ui_dirty_bottom) {
            draw_prompt_status_pane(ctx, &input_ctx);
            ui_dirty_bottom = 0;
        }

        int poll_result = poll(&pfd, 1, 20); // 20ms timeout
                                                     
        if (poll_result > 0 && (pfd.revents & POLLIN)) {
            // Input is available, use non-blocking read
            ssize_t n = read(FD, &c, 1);
            if (n > 0) {
                input_handler(ctx, &input_ctx, c);
            }
        }
        // Check for messages from the motor
        struct clean_tel_msg_s tel_msg;
        struct timespec timeout;
        mc_get_abstime_from_now(&timeout, 20); // 20ms in the future
        // Wait for a message from the motor, but continue after the timeout (deadline)
        //ssize_t bytes_received = mq_receive(tel_q, (char *)&tel_msg, sizeof(tel_msg), NULL);
        ssize_t bytes_received = mq_timedreceive(tel_q, (char *)&tel_msg, sizeof(tel_msg), NULL, &timeout);
        if (bytes_received > 0) {
                switch (tel_msg.state) {
                // TODO: handle time remaining, state, and other message fields
                case MOTOR_STATE_STOPPED:
                    /* Reset countdown to default value */
                    snprintf(ctx->str_status, sizeof(ctx->str_status), "XMotor stopped");
                    snprintf(ctx->str_time_remaining, sizeof(ctx->str_time_remaining), "%02d:%02d", tel_msg.time_remaining / 60, tel_msg.time_remaining % 60);
                    ui_dirty_bottom = 1;
                    break; 
                case MOTOR_STATE_RUNNING:
                    /* Update the status and time remaining */
                    snprintf(ctx->str_status, sizeof(ctx->str_status), "XMotor running");
                    snprintf(ctx->str_time_remaining, sizeof(ctx->str_time_remaining), "%02d:%02d", tel_msg.time_remaining / 60, tel_msg.time_remaining % 60);
                    ui_dirty_bottom = 1;
                    break; 
                case MOTOR_STATE_PAUSED:
                    snprintf(ctx->str_status, sizeof(ctx->str_status), "XMotor paused");
                    snprintf(ctx->str_time_remaining, sizeof(ctx->str_time_remaining), "%02d:%02d", tel_msg.time_remaining / 60, tel_msg.time_remaining % 60);
                    ui_dirty_bottom = 1;
                    break; 
                case MOTOR_STATE_ABORTED:
                    snprintf(ctx->str_status, sizeof(ctx->str_status), "XMotor run aborted");
                    snprintf(ctx->str_time_remaining, sizeof(ctx->str_time_remaining), "%02d:%02d", tel_msg.time_remaining / 60, tel_msg.time_remaining % 60);
                    ui_dirty_bottom = 1;
                    break; 
                case MOTOR_MESSAGE:
                    snprintf(ctx->str_status, sizeof(ctx->str_status), "%s", g_motor_messages[tel_msg.message_id]);
                    snprintf(ctx->str_time_remaining, sizeof(ctx->str_time_remaining), "%02d:%02d", g_duration / 60, g_duration % 60);
                    ui_dirty_bottom = 1;
                    break;
                default:
                    snprintf(ctx->str_status, sizeof(ctx->str_status), "XMotor unknown message (%d)", tel_msg.state);
                    ui_dirty_bottom = 1;
                    break;
                }
        }
        usleep(20000); // 20ms
    }

    disable_raw_mode(ctx);
    close(FD);
    // Send a quit message to the motor task
    struct clean_cmd_msg_s quit_msg = {
        .command = MSG_STOP_REQ
    };
    mq_send(cmd_q, (const char *)&quit_msg, sizeof(quit_msg), 0);
    mq_close(cmd_q);
    mq_close(tel_q);

    return 0;
}
