#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <dirent.h>
#include <errno.h>

#define DEFAULT_TASK_DIRECTORY "./tasks/"
#define DEFAULT_TASK_FILENAME "/task.md"

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

typedef enum {
    TASK_OPEN,
    TASK_CLOSED,
    TASK_UNKNOWN
} TaskStatus;

typedef enum {
    COMMAND_CREATE,
    COMMAND_LS,
    COMMAND_INIT,
    COMMAND_UNKNOWN
} CommandType;

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

bool mkdir_if_not_exists(char* filepath)
{
    int result = mkdir(filepath, 0700);
    if (result < 0) {
	if (errno == EEXIST) {
	    printf("Directory %s already exists\n", filepath);
	} else {
	    printf("Couldn't create directory %s: %s\n", filepath, strerror(errno));
	}
	return false;
    }
    return true;
}

char* task_status_as_string(TaskStatus status)
{
    switch(status) {
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

char* command_type_as_string(CommandType command)
{
    switch(command) {
	case COMMAND_CREATE:
	    return "create";
	    break;
	case COMMAND_LS:
	    return "ls";
	    break;
	case COMMAND_INIT:
	    return "init";
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
    struct tm * tm_p = localtime(&timer);
    if (tm_p) tm_info = *tm_p;

    int buffer_size = 1024;
    char* time_string = malloc(buffer_size); 
    snprintf(time_string, buffer_size, "%d%02d%02d-%02d%02d%02d", 
	     tm_info.tm_year + 1900, tm_info.tm_mon + 1, tm_info.tm_mday, tm_info.tm_hour, tm_info.tm_min, tm_info.tm_sec);
    return time_string;
}

bool write_task_to_file(char* filepath, Task* task)
{
    FILE* task_file = fopen(filepath, "w");
    if (task_file == NULL) {
	printf("Couldn't open file %s: %s\n", filepath, strerror(errno));
	return false;
    } 

    fprintf(task_file, "%s\n", task->description);
    fprintf(task_file, "%s\n", task_status_as_string(task->status));
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
    if (task_file == NULL) {
	// return 0-defined structure on error
	printf("Couldn't open file %s: %s\n", filepath, strerror(errno));
	return task;
    } 

    while ((read = getline(&line, &n, task_file)) != -1) {
	line[strlen(line) - 1] = '\0';
	if (counter == 0) {
	    task.description = strdup(line);
	}
	if (counter == 1) {
	    task.status = string_as_task_status(line);
	}
	if (counter == 2) {
	    task.tags = strdup(line);
	}
	if (counter == 3) {
	    task.priority = strtol(strdup(line), NULL, 10);
	}
	counter++;
    }
    fclose(task_file);
    free(line);
    return task;
}

bool print_tasks()
{
    Tasks tasks = {0};
    struct dirent* de;
    DIR* dir = opendir(DEFAULT_TASK_DIRECTORY);
    if (dir == NULL) {
	printf("Couldn't open directory %s: %s\n", DEFAULT_TASK_DIRECTORY, strerror(errno));
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
	strcat(path, DEFAULT_TASK_FILENAME);

	Task task = read_task_from_file(path);
	if (task.description == 0) continue; // can't properly read file 
	task.path = strdup(path);
	da_append(&tasks, task); // add each task to dynamic array
	free(path);
    }

    qsort(tasks.items, tasks.count, sizeof(Task), task_compare);

    for (size_t i = 0; i < tasks.count; i++) {
	Task task = tasks.items[i];
	printf("[%s] [%d] (%s) [%s] - %s\n", task.path, task.priority, task_status_as_string(task.status), task.tags, task.description);	
    }

    closedir(dir); 
    return true;
}

bool create_task(int priority, char* description, char* tags)
{
    char* current_time = get_current_time();
    char* path = malloc(strlen(current_time) + strlen(DEFAULT_TASK_DIRECTORY) + strlen(DEFAULT_TASK_FILENAME) + 1);

    strcpy(path, DEFAULT_TASK_DIRECTORY);
    strcat(path, current_time);
    mkdir_if_not_exists(path);

    strcat(path, DEFAULT_TASK_FILENAME);
    Task task = {description, TASK_OPEN, tags, priority, path};
    bool write_result = write_task_to_file(path, &task);
    if (!write_result) return false;

    free(path);

    return true;
}

void print_help()
{
    Command commands[] = {
	{COMMAND_CREATE, "<PRIORITY> <DESCRIPTION> <TAGS>", "creates new task with given priority, description and tags"},
	{COMMAND_LS, "", "shows all available tasks"},
	{COMMAND_INIT, "", "creates new directory `tasks` in current directory"},
    };

    size_t commands_amount = sizeof(commands) / sizeof(commands[0]);

    printf("usage: track <COMMAND>\n");
    printf("available commands:\n");

    for (size_t i = 0; i < commands_amount; i++) {
	printf("\t%s %s - %s\n", command_type_as_string(commands[i].type), commands[i].params, commands[i].description);
    }
}

int main(int argc, char** argv)
{
    if (argc == 1) {
	print_help();	
	return 1;
    }

    char* command_string = argv[1];
    CommandType command = string_as_command_type(command_string);

    switch (command) {
	case COMMAND_CREATE:
	    if (argc != 5) {
		print_help();
		return 1;
	    }
	    int priority = strtol(argv[2], NULL, 10);
	    char* description = argv[3];
	    char* tags = argv[4];
	    bool create_task_result = create_task(priority, description, tags); 
	    if (!create_task_result) return 1;
	    break;
	case COMMAND_LS:
	    bool print_task_result = print_tasks();
	    if (!print_task_result) return 1;
	    break;
	case COMMAND_INIT:
	    mkdir_if_not_exists(DEFAULT_TASK_DIRECTORY);
	    break;
	default:
	    print_help();
	    break;
    }
    return 0;
}
