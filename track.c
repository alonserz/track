#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <dirent.h>
#include <errno.h>
#include <unistd.h>

#define DEFAULT_TASK_DIRECTORY "./tasks/"
#define DEFAULT_TASK_FILENAME "/task.md"
#define DEFAULT_TIME_LABEL_FILENAME ".timelabel"

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"

#define da_append(xs, x)                                                             \
    do {                                                                             \
        if ((xs)->count >= (xs)->capacity) {                                         \
            if ((xs)->capacity == 0) (xs)->capacity = 256;                           \
            else (xs)->capacity *= 2;                                                \
            (xs)->items = realloc((xs)->items, (xs)->capacity*sizeof(*(xs)->items)); \
        }                                                                            \
                                                                                     \
        (xs)->items[(xs)->count++] = (x);                                            \
    } while (0)

static int NO_COLOR = 0;

typedef enum {
    TASK_OPEN,
    TASK_CLOSED,
    TASK_UNKNOWN
} TaskStatus;

typedef enum {
    COMMAND_CREATE,
    COMMAND_LS,
    COMMAND_INIT,
    COMMAND_LABEL,
    COMMAND_OPEN,
    COMMAND_CLOSE,
    COMMAND_UNKNOWN
} CommandType;

typedef enum {
    OPERATOR_GE, // greater or equals
    OPERATOR_LE, // less or equals
    OPERATOR_EQ, // equals
    OPERATOR_GT, // less than
    OPERATOR_LT, // greater than
} Operator;

typedef struct {
    CommandType type;
    char* params;
    char* description;
} Command;

typedef struct {
    char* description;
    TaskStatus status;
    char* tags;
    int priority;
    char* path;
} Task;

typedef struct {
    Task* items;
    size_t count;
    size_t capacity;
} Tasks;

typedef struct {
    int days;
    int hours;
    int minutes;
    int seconds;
} Timestamp;

typedef struct {
    TaskStatus status;
    struct {
	Operator operator;
	int value;
    } priority;
} Filter;

bool mkdir_if_not_exists(char* filepath)
{
    int result = mkdir(filepath, 0700);
    if (result < 0) return false;
    return true;
}

char* task_status_as_string(TaskStatus* status)
{
    switch(*status) {
	case TASK_OPEN:
	    return "OPEN";
	    break;
	case TASK_CLOSED:
	    return "CLOSED";
	    break;
	default:
	    return "UNKNOWN";
	    break;
    }
}

TaskStatus string_as_task_status(char* string)
{
    if (strcmp(string, "OPEN") == 0)   return TASK_OPEN;
    if (strcmp(string, "CLOSED") == 0) return TASK_CLOSED;
    return TASK_UNKNOWN;
}

char* command_type_as_string(CommandType* command)
{
    switch(*command) {
	case COMMAND_CREATE:
	    return "create";
	    break;
	case COMMAND_LS:
	    return "ls";
	    break;
	case COMMAND_INIT:
	    return "init";
	    break;
	case COMMAND_LABEL:
	    return "label";
	    break;
	case COMMAND_OPEN:
	    return "open";
	    break;
	case COMMAND_CLOSE:
	    return "close";
	    break;
	default:
	    return "unknown";
	    break;
    }
}

CommandType string_as_command_type(char* string)
{
    if (strcmp(string, "create") == 0)  return COMMAND_CREATE;
    if (strcmp(string, "ls") == 0) 	return COMMAND_LS;
    if (strcmp(string, "init") == 0) 	return COMMAND_INIT;
    if (strcmp(string, "label") == 0) 	return COMMAND_LABEL;
    if (strcmp(string, "open") == 0) 	return COMMAND_OPEN;
    if (strcmp(string, "close") == 0) 	return COMMAND_CLOSE;
    return COMMAND_UNKNOWN;

}

int task_compare(const void* s1, const void* s2)
{
    Task* t1 = (Task*)s1;
    Task* t2 = (Task*)s2;

    return -1 * (t1->priority - t2->priority);
}

char* get_current_time()
{
    time_t timer = time(NULL);
    struct tm tm_info = {0};
    struct tm* tm_p = localtime(&timer);
    if (tm_p) tm_info = *tm_p;

    int buffer_size = 1024;
    char* time_string = malloc(buffer_size); 
    snprintf(time_string, buffer_size, "%d%02d%02d-%02d%02d%02d", 
	     tm_info.tm_year + 1900, tm_info.tm_mon + 1, tm_info.tm_mday, tm_info.tm_hour, tm_info.tm_min, tm_info.tm_sec);
    return time_string;
}

void convert_seconds_to_timestamp(Timestamp* timestamp, int seconds)
{
    int days = seconds / 86400;
    seconds = seconds % 86400;

    int hours = seconds / 3600;
    seconds %= 3600;

    int minutes = seconds / 60;
    int secs = seconds % 60;

    timestamp->days = days;
    timestamp->hours = hours;
    timestamp->minutes = minutes;
    timestamp->seconds = secs;
}


bool write_task_to_file(char* filepath, Task* task, char* mode)
{
    FILE* task_file = fopen(filepath, mode);
    if (task_file == NULL) return false;

    fprintf(task_file, "%s\n", task->description);
    fprintf(task_file, "%s\n", task_status_as_string(&task->status));
    fprintf(task_file, "%s\n", task->tags);
    fprintf(task_file, "%d\n", task->priority);

    fclose(task_file);
    return true;
}

Task read_task_from_file(char* filepath)
{
    char* line = NULL;
    size_t n = 0;
    ssize_t read;
    int counter = 0;

    Task task = {0};

    FILE* task_file = fopen(filepath, "r");
    if (task_file == NULL) return task;

    while ((read = getline(&line, &n, task_file)) != -1) {
	line[strlen(line) - 1] = '\0';
	if 	(counter == 0) task.description = strdup(line);
	else if (counter == 1) task.status = string_as_task_status(line);
	else if (counter == 2) task.tags = strdup(line);
	else if (counter == 3) task.priority = strtol(line, NULL, 10);
	counter++;
    }
    fclose(task_file);
    free(line);
    return task;
}

bool create_task(int priority, char* description, char* tags)
{
    char* current_time = get_current_time();
    char* path = malloc(strlen(current_time) + strlen(DEFAULT_TASK_DIRECTORY) + strlen(DEFAULT_TASK_FILENAME) + 1);

    strcpy(path, DEFAULT_TASK_DIRECTORY);
    strcat(path, current_time);
    bool status = mkdir_if_not_exists(path);
    if (!status) return false;

    strcat(path, DEFAULT_TASK_FILENAME);
    Task task = {description, TASK_OPEN, tags, priority, path};
    bool write_result = write_task_to_file(path, &task, "w");
    if (!write_result) return false;

    free(path);
    free(current_time);

    return true;
}

bool update_label(char* dirpath)
{
    if (dirpath[strlen(dirpath) - 1] != '/') {
	strcat(dirpath, "/");
    }

    DIR* dir = opendir(dirpath);
    if 	 (dir != NULL) closedir(dir); 
    else return false;

    char* path = strdup(dirpath);
    strcat(path, DEFAULT_TIME_LABEL_FILENAME);

    FILE* time_label_file = fopen(path, "a+");
    if (time_label_file == NULL) return false;
    fprintf(time_label_file, "%d\n", (int)time(NULL));

    fclose(time_label_file);
    free(path);
    return true;
}

bool update_task_status(char* dirpath, TaskStatus task_status)
{
    if (dirpath[strlen(dirpath) - 1] != '/') {
	strcat(dirpath, "/");
    }

    DIR* dir = opendir(dirpath);
    if 	 (dir != NULL) closedir(dir); 
    else return false;

    char* path = strdup(dirpath);
    strcat(path, DEFAULT_TASK_FILENAME);

    Task task = read_task_from_file(path);
    if (task.description == 0) return false;
    task.status = task_status;
    bool write_result = write_task_to_file(path, &task, "r+");
    if (!write_result) return false;

    free(path);
    return true;
}

Filter parse_filter_options(char* filter_string)
{
    Filter filter = {0};
    char* field = malloc(50);
    char* operator = malloc(3);
    char* value = malloc(10);
    // filter string example:
    // "priority > 50";
    int result = sscanf(filter_string, "%s %[<>=] %s", field, operator, value);

    if (result != 3) return filter;

    if (strcmp(field, "priority") == 0) {
	if 	(strcmp(operator, "<=") == 0) filter.priority.operator = OPERATOR_LE;
	else if (strcmp(operator, "==") == 0) filter.priority.operator = OPERATOR_EQ;
	else if (strcmp(operator, "<") == 0)  filter.priority.operator = OPERATOR_LT;
	else if (strcmp(operator, ">") == 0)  filter.priority.operator = OPERATOR_GT;
	else 	filter.priority.operator = OPERATOR_GE;
	filter.priority.value = strtol(value, NULL, 10); 
    } 

    if (strcmp(field, "status")   == 0) filter.status = string_as_task_status(value);

    free(field);
    free(operator);
    free(value);
    
    return filter;
}

bool filter_task(Task* task, Filter* filter)
{
    bool result = false;

    if (filter->priority.operator == OPERATOR_LE) {
	if (task->priority <= filter->priority.value) result = true;
    }
    else if (filter->priority.operator == OPERATOR_EQ) {
	if (task->priority == filter->priority.value) result = true;
    }
    else if (filter->priority.operator == OPERATOR_GE) {
	if (task->priority >= filter->priority.value) result = true;
    }
    else if (filter->priority.operator == OPERATOR_GT) {
	if (task->priority > filter->priority.value) result = true;
    }
    else if (filter->priority.operator == OPERATOR_LT) {
	if (task->priority < filter->priority.value) result = true;
    }

    if (filter->status == task->status) {
	if (result) result = true;
    } else {
	result = false;
    }

    return result;
}

char* COLOR(char* str, char* color)
{
    int buffer_size = 1024;
    char* colored_string = malloc(buffer_size); 
    if (!NO_COLOR) { 
	snprintf(colored_string, buffer_size, "%s%s%s", color, str, ANSI_COLOR_RESET); 
    } else {
	strcpy(colored_string, str);
    }
    return colored_string;
}

bool print_tasks(FILE* stream, Filter* filter)
{
    Tasks tasks = {0};
    struct dirent* de;
    DIR* dir = opendir(DEFAULT_TASK_DIRECTORY);
    if (dir == NULL) {
	fprintf(stream, "Couldn't open directory %s: %s\n", DEFAULT_TASK_DIRECTORY, strerror(errno));
	return false;
    }

    while ((de = readdir(dir)) != NULL) {
	char* directory = de->d_name;
	// skip . and .. directories
	if (strcmp(directory, "..") == 0) continue;
	if (strcmp(directory, ".")  == 0) continue;

	char* path = malloc(strlen(DEFAULT_TASK_DIRECTORY) + strlen(directory) + strlen(DEFAULT_TASK_FILENAME) + 1);
	strcpy(path, DEFAULT_TASK_DIRECTORY);
	strcat(path, directory);

	// Copying directory path to save it later
	char* dirpath = malloc(strlen(path) + 2);
	strcpy(dirpath, path);
	strcat(dirpath, "/");

	strcat(path, DEFAULT_TASK_FILENAME);

	Task task = read_task_from_file(path);
	if (task.description == 0) continue;
	if (!filter_task(&task, filter)) continue;
	
	// saving path to task directory
	task.path = strdup(dirpath);
	da_append(&tasks, task); // add each task to dynamic array
	free(path);
	free(dirpath);
    }

    qsort(tasks.items, tasks.count, sizeof(Task), task_compare);
    closedir(dir); 

    for (size_t i = 0; i < tasks.count; i++) {
	Task task = tasks.items[i];
	char* task_status = task_status_as_string(&task.status);

	if (strcmp(task_status, "OPEN") == 0) task_status = COLOR(task_status, ANSI_COLOR_GREEN);
	else task_status = COLOR(task_status, ANSI_COLOR_RED);

	char* task_path = COLOR(task.path, ANSI_COLOR_RED); 
	fprintf(stream, "[%s] [%d] (%s) [%s] - %s\n", task_path, task.priority, task_status, task.tags, task.description);

	free(task_path);
	free(task_status);

	char* time_label_filepath = malloc(strlen(task.path) + strlen(DEFAULT_TIME_LABEL_FILENAME) + 1);
	strcpy(time_label_filepath, task.path);
	strcat(time_label_filepath, DEFAULT_TIME_LABEL_FILENAME);

	if (access(time_label_filepath, F_OK) == 0) {
	    FILE* time_label_file = fopen(time_label_filepath, "r");
	    if (time_label_file == NULL) {
		fprintf(stream, "Couldn't open file %s: %s\n", time_label_filepath, strerror(errno));
		return false;
	    }

	    char* line = NULL;
	    size_t n = 0;
	    ssize_t read;
	    int counter = 0;

	    int label0 = 0;
	    int label1 = 0;
	    int seconds_delta = 0;

	    while ((read = getline(&line, &n, time_label_file)) != -1) {
		line[strlen(line) - 1] = '\0';
		if (counter % 2 == 0) {
		    label0 = strtol(line, NULL, 10); 
		} else {
		    label1 = strtol(line, NULL, 10);
		    seconds_delta += difftime(label1, label0);
		}
		counter++;
	    }

	    fclose(time_label_file);
	
	    // if label0 was set, but not label1 
	    if (label0 > label1) {
		seconds_delta += difftime(time(NULL), label0);
		char* active_prefix_string = COLOR("[Active] ", ANSI_COLOR_BLUE);
		fprintf(stream, "%s", active_prefix_string);
		free(active_prefix_string);
	    }

	    Timestamp timestamp = {0};
	    convert_seconds_to_timestamp(&timestamp, seconds_delta);
	    free(line);

	    fprintf(stream, "Time expired: %d days %d hours %d minutes %d seconds\n", timestamp.days, timestamp.hours, timestamp.minutes, timestamp.seconds);
	}
	free(time_label_filepath);
    }
    // free dynamic array;
    free(tasks.items);
    tasks.items = NULL;
    tasks.capacity = 0;
    tasks.count = 0;
    return true;
}

void print_help(FILE* stream)
{
    Command commands[] = {
	{COMMAND_CREATE, "<PRIORITY> <DESCRIPTION> <TAGS>", "creates new task with given priority, description and tags"},
	{COMMAND_LS, "<OPTION>", "shows all available tasks. \n\t\tAvailable options:\
							     \n\t\t--no-color - turns off colors\
							     \n\t\t--filter <<field> <operator> <value>> - filter output with given string. Example `--filter 'priority > 50'`"},
	{COMMAND_INIT, "", "creates new directory `tasks` in current directory"},
	{COMMAND_LABEL, "<DIRPATH>", "Update time label"},
	{COMMAND_OPEN, "<DIRPATH>", "Set task status to OPEN"},
	{COMMAND_CLOSE, "<DIRPATH>", "Set task status to CLOSED"},
    };

    size_t commands_amount = sizeof(commands) / sizeof(commands[0]);

    fprintf(stream, "usage: track <COMMAND>\n");
    fprintf(stream, "available commands:\n");

    for (size_t i = 0; i < commands_amount; i++) {
	fprintf(stream, "\t%s %s - %s\n", command_type_as_string(&commands[i].type), commands[i].params, commands[i].description);
    }
}

int main(int argc, char** argv)
{
    if (argc == 1) {
	print_help(stdout);
	return 1;
    }

    FILE* stream = stdout;
    char* command_string = argv[1];
    CommandType command = string_as_command_type(command_string);

    switch (command) {
	case COMMAND_CREATE:
	    if (argc != 5) {
		print_help(stream);
		return 1;
	    }
	    int priority = strtol(argv[2], NULL, 10);
	    char* description = argv[3];
	    char* tags = argv[4];
	    bool create_task_result = create_task(priority, description, tags); 
	    if (!create_task_result) {
		fprintf(stream, "Couldn't create task: %s\n", strerror(errno));
		return 1;
	    };
	    break;
	case COMMAND_LS:
	    Filter filter = { 0 };
	    if (argc >= 3) {
		for (size_t i = 0; i < (size_t)argc; i++) {
		    if (strcmp(argv[i], "--no-color") == 0) NO_COLOR = 1;
		    if (strcmp(argv[i], "--filter")   == 0) {
			if ((size_t)(argc - 1) != i) filter = parse_filter_options(argv[i + 1]);
		    } 
		}
	    }
	    bool print_task_result = print_tasks(stream, &filter);
	    if (!print_task_result) return 1;
	    break;
	case COMMAND_INIT:
	    bool mkdir_status = mkdir_if_not_exists(DEFAULT_TASK_DIRECTORY);
	    if (!mkdir_status) {
		fprintf(stream, "Couldn't initialize directory `%s`: %s\n", DEFAULT_TASK_DIRECTORY, strerror(errno));
		return 1;
	    }
	    break;
	case COMMAND_LABEL:
	    if (argc != 3) {
		print_help(stream);
		return 1;
	    }
	    char* dirpath = argv[2];
	    bool update_label_status = update_label(dirpath);
	    if (!update_label_status) {
		fprintf(stream, "Couldn't add new label to task `%s`: %s\n", DEFAULT_TASK_DIRECTORY, strerror(errno));
		return 1;
	    }
	    break;
	case COMMAND_OPEN:
	    if (argc != 3) {
		print_help(stream);
		return 1;
	    }
	    char* task_open_dirpath = argv[2];
	    bool open_task_result = update_task_status(task_open_dirpath, TASK_OPEN);

	    if (!open_task_result) {
		fprintf(stream, "Couldn't set status OPEN for task `%s`: %s\n", task_open_dirpath, strerror(errno));
		return 1;
	    }
	    break;
	case COMMAND_CLOSE:
	    if (argc != 3) {
		print_help(stream);
		return 1;
	    }
	    char* task_close_dirpath = argv[2];
	    bool close_task_result = update_task_status(task_close_dirpath, TASK_CLOSED);
	    if (!close_task_result) {
		fprintf(stream, "Couldn't set status OPEN for task `%s`: %s\n", task_close_dirpath, strerror(errno));
		return 1;
	    }
	    break;
	default:
	    print_help(stream);
	    break;
    }
    return 0;
}
