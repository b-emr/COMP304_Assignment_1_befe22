#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <termios.h> // termios, TCSANOW, ECHO, ICANON
#include <unistd.h>
const char *sysname = "shellish";

enum return_codes {
  SUCCESS = 0,
  EXIT = 1,
  UNKNOWN = 2,
};

struct command_t {
  char *name;
  bool background;
  bool auto_complete;
  int arg_count;
  char **args;
  char *redirects[3];     // in/out redirection
  struct command_t *next; // for piping
};

/**
 * Prints a command struct
 * @param struct command_t *
 */
void print_command(struct command_t *command) {
  int i = 0;
  printf("Command: <%s>\n", command->name);
  printf("\tIs Background: %s\n", command->background ? "yes" : "no");
  printf("\tNeeds Auto-complete: %s\n", command->auto_complete ? "yes" : "no");
  printf("\tRedirects:\n");
  for (i = 0; i < 3; i++)
    printf("\t\t%d: %s\n", i,
           command->redirects[i] ? command->redirects[i] : "N/A");
  printf("\tArguments (%d):\n", command->arg_count);
  for (i = 0; i < command->arg_count; ++i)
    printf("\t\tArg %d: %s\n", i, command->args[i]);
  if (command->next) {
    printf("\tPiped to:\n");
    print_command(command->next);
  }
}

/**
 * Release allocated memory of a command
 * @param  command [description]
 * @return         [description]
 */
int free_command(struct command_t *command) {
  if (command->arg_count) {
    for (int i = 0; i < command->arg_count; ++i)
      free(command->args[i]);
    free(command->args);
  }
  for (int i = 0; i < 3; ++i)
    if (command->redirects[i])
      free(command->redirects[i]);
  if (command->next) {
    free_command(command->next);
    command->next = NULL;
  }
  free(command->name);
  free(command);
  return 0;
}

/**
 * Show the command prompt
 * @return [description]
 */
int show_prompt() {
  char cwd[1024], hostname[1024];
  gethostname(hostname, sizeof(hostname));
  getcwd(cwd, sizeof(cwd));
  printf("%s@%s:%s %s$ ", getenv("USER"), hostname, cwd, sysname);
  return 0;
}

/**
 * Parse a command string into a command struct
 * @param  buf     [description]
 * @param  command [description]
 * @return         0
 */
int parse_command(char *buf, struct command_t *command) {
  const char *splitters = " \t"; // split at whitespace
  int index, len;
  len = strlen(buf);
  while (len > 0 && strchr(splitters, buf[0]) != NULL) // trim left whitespace
  {
    buf++;
    len--;
  }
  while (len > 0 && strchr(splitters, buf[len - 1]) != NULL)
    buf[--len] = 0; // trim right whitespace

  if (len > 0 && buf[len - 1] == '?') // auto-complete
    command->auto_complete = true;
  if (len > 0 && buf[len - 1] == '&') // background
    command->background = true;

  char *pch = strtok(buf, splitters);
  if (pch == NULL) {
    command->name = (char *)malloc(1);
    command->name[0] = 0;
  } else {
    command->name = (char *)malloc(strlen(pch) + 1);
    strcpy(command->name, pch);
  }

  command->args = (char **)malloc(sizeof(char *));

  int redirect_index;
  int arg_index = 0;
  char temp_buf[1024], *arg;
  while (1) {
    // tokenize input on splitters
    pch = strtok(NULL, splitters);
    if (!pch)
      break;
    arg = temp_buf;
    strcpy(arg, pch);
    len = strlen(arg);

    if (len == 0)
      continue; // empty arg, go for next
    while (len > 0 && strchr(splitters, arg[0]) != NULL) // trim left whitespace
    {
      arg++;
      len--;
    }
    while (len > 0 && strchr(splitters, arg[len - 1]) != NULL)
      arg[--len] = 0; // trim right whitespace
    if (len == 0)
      continue; // empty arg, go for next

    // piping to another command
    if (strcmp(arg, "|") == 0) {
      struct command_t *c =
          (struct command_t *)malloc(sizeof(struct command_t));
      int l = strlen(pch);
      pch[l] = splitters[0]; // restore strtok termination
      index = 1;
      while (pch[index] == ' ' || pch[index] == '\t')
        index++; // skip whitespaces

      parse_command(pch + index, c);
      pch[l] = 0; // put back strtok termination
      command->next = c;
      continue;
    }

    // background process
    if (strcmp(arg, "&") == 0)
      continue; // handled before

    // handle input redirection
    redirect_index = -1;
    if (arg[0] == '<')
      redirect_index = 0;
    if (arg[0] == '>') {
      if (len > 1 && arg[1] == '>') {
        redirect_index = 2;
        arg++;
        len--;
      } else
        redirect_index = 1;
    }
    if (redirect_index != -1) {
      command->redirects[redirect_index] = (char *)malloc(len);
      strcpy(command->redirects[redirect_index], arg + 1);
      continue;
    }

    // normal arguments
    if (len > 2 &&
        ((arg[0] == '"' && arg[len - 1] == '"') ||
         (arg[0] == '\'' && arg[len - 1] == '\''))) // quote wrapped arg
    {
      arg[--len] = 0;
      arg++;
    }
    command->args =
        (char **)realloc(command->args, sizeof(char *) * (arg_index + 1));
    command->args[arg_index] = (char *)malloc(len + 1);
    strcpy(command->args[arg_index++], arg);
  }
  command->arg_count = arg_index;

  // increase args size by 2
  command->args = (char **)realloc(command->args,
                                   sizeof(char *) * (command->arg_count += 2));

  // shift everything forward by 1
  for (int i = command->arg_count - 2; i > 0; --i)
    command->args[i] = command->args[i - 1];

  // set args[0] as a copy of name
  command->args[0] = strdup(command->name);
  // set args[arg_count-1] (last) to NULL
  command->args[command->arg_count - 1] = NULL;

  return 0;
}

void prompt_backspace() {
  putchar(8);   // go back 1
  putchar(' '); // write empty over
  putchar(8);   // go back 1 again
}

/**
 * Prompt a command from the user
 * @param  buf      [description]
 * @param  buf_size [description]
 * @return          [description]
 */
int prompt(struct command_t *command) {
  int index = 0;
  char c;
  char buf[4096];
  static char oldbuf[4096];

  // tcgetattr gets the parameters of the current terminal
  // STDIN_FILENO will tell tcgetattr that it should write the settings
  // of stdin to oldt
  static struct termios backup_termios, new_termios;
  tcgetattr(STDIN_FILENO, &backup_termios);
  new_termios = backup_termios;
  // ICANON normally takes care that one line at a time will be processed
  // that means it will return if it sees a "\n" or an EOF or an EOL
  new_termios.c_lflag &=
      ~(ICANON |
        ECHO); // Also disable automatic echo. We manually echo each char.
  // Those new settings will be set to STDIN
  // TCSANOW tells tcsetattr to change attributes immediately.
  tcsetattr(STDIN_FILENO, TCSANOW, &new_termios);

  show_prompt();
  buf[0] = 0;
  while (1) {
    c = getchar();
    // printf("Keycode: %u\n", c); // DEBUG: uncomment for debugging

    if (c == 9) // handle tab
    {
      buf[index++] = '?'; // autocomplete
      break;
    }

    if (c == 127) // handle backspace
    {
      if (index > 0) {
        prompt_backspace();
        index--;
      }
      continue;
    }

    if (c == 27 || c == 91 || c == 66 || c == 67 || c == 68) {
      continue;
    }

    if (c == 65) // up arrow
    {
      while (index > 0) {
        prompt_backspace();
        index--;
      }

      char tmpbuf[4096];
      printf("%s", oldbuf);
      strcpy(tmpbuf, buf);
      strcpy(buf, oldbuf);
      strcpy(oldbuf, tmpbuf);
      index += strlen(buf);
      continue;
    }

    putchar(c); // echo the character
    buf[index++] = c;
    if (index >= sizeof(buf) - 1)
      break;
    if (c == '\n') // enter key
      break;
    if (c == 4) // Ctrl+D
      return EXIT;
  }
  if (index > 0 && buf[index - 1] == '\n') // trim newline from the end
    index--;
  buf[index++] = '\0'; // null terminate string

  strcpy(oldbuf, buf);

  parse_command(buf, command);

  // print_command(command); // DEBUG: uncomment for debugging

  // restore the old settings
  tcsetattr(STDIN_FILENO, TCSANOW, &backup_termios);
  return SUCCESS;
}

char *find_executable(const char *name) {
  if (strchr(name, '/')) {
    if (access(name, X_OK) == 0)
      return strdup(name);
    return NULL;
  }

  const char *pathenv = getenv("PATH");
  if (!pathenv)
    return NULL;

  char *paths = strdup(pathenv);
  char *dir = strtok(paths, ":");
  while (dir) {
    char *full = malloc(strlen(dir) + 1 + strlen(name) + 1);
    if (!full)
      break;
    sprintf(full, "%s/%s", dir, name);
    if (access(full, X_OK) == 0) {
      free(paths);
      return full;
    }
    free(full);
    dir = strtok(NULL, ":");
  }
  free(paths);
  return NULL;
}

int process_command(struct command_t *command) {
  int r;
  if (strcmp(command->name, "") == 0)
    return SUCCESS;

  if (strcmp(command->name, "exit") == 0)
    return EXIT;

  if (strcmp(command->name, "cd") == 0) {
    if (command->arg_count > 0) {
      r = chdir(command->args[1]);
      if (r == -1)
        printf("-%s: %s: %s\n", sysname, command->name, strerror(errno));
      return SUCCESS;
    }
  }

  // cut commend implementation
  if (strcmp(command->name, "cut") == 0) {
    char delim = '\t';
    char *fields = NULL;
    
    for (int i = 1; i < command->arg_count - 1; i++) {
      if (strcmp(command->args[i], "-d") == 0 || strcmp(command->args[i], "--delimiter") == 0) {
        delim = command->args[i + 1][0];
        i++;
      } 
      else if (strcmp(command->args[i], "-f") == 0 || strcmp(command->args[i], "--fields") == 0) {
        fields = command->args[i + 1];
        i++;
      }
    }
    
    if (!fields) {
      printf("-%s: %s: %s\n", sysname, command->name, strerror(errno));
      return SUCCESS;
    }
    
    int *fields_arr = (int *) malloc(sizeof(int) * 100);
    int field_count = 0;
    char *fields_copy = strdup(fields);
    char *token = strtok(fields_copy, ",");
    while (token && field_count < 100) {
      fields_arr[field_count++] = atoi(token);
      token = strtok(NULL, ",");
    }
    free(fields_copy);
    
    FILE *input = stdin;
    
    char line[1024];
    while (fgets(line, sizeof(line), input)) {
      int len = strlen(line);
      if (len > 0 && line[len - 1] == '\n') {
        line[len - 1] = '\0';
      }
      
      char *line_copy = strdup(line);
      char **fields_data = (char **)malloc(sizeof(char *) * 100);
      int num_fields = 0;
      
      char *start = line_copy;
      for (char *p = line_copy; *p; p++) {
        if (*p == delim) {
          *p = '\0';
          fields_data[num_fields++] = strdup(start);
          start = p + 1;
        }
      }
      fields_data[num_fields++] = strdup(start);
      
      free(line_copy);
      
      for (int i = 0; i < field_count; i++) {
        int field_idx = fields_arr[i] - 1;
        if (field_idx >= 0 && field_idx < num_fields) {
          if (i > 0) printf("%c", delim);
          printf("%s", fields_data[field_idx]);
        }
      }
      printf("\n");
      
      for (int i = 0; i < num_fields; i++) {
        free(fields_data[i]);
      }
      free(fields_data);
    }
    
    free(fields);
    free(fields_arr);
    return SUCCESS;
  }

  // chatroom command implementation
  if (strcmp(command->name, "chatroom") == 0) {
    if (command->arg_count < 3) {
      printf("-%s: %s: %s\n", sysname, command->name, strerror(errno));
      return SUCCESS;
    }
    
    char *roomname = command->args[1];
    char *username = command->args[2];
    char room_path[256], user_path[256];
    char command[512];
    
    snprintf(room_path, sizeof(room_path), "/tmp/chatroom-%s", roomname);
    snprintf(user_path, sizeof(user_path), "/tmp/chatroom-%s/%s", roomname, username);
    
    snprintf(command, sizeof(command), "mkdir -p %s; mkfifo %s", room_path, user_path);
    system(command);

    printf("Welcome to %s!\n", roomname);
    
    pid_t reader = fork();
    if (reader == 0) { // reader child
      snprintf(command, sizeof(command), "tail -f %s", user_path);
      system(command);
      exit(0);
    }
    
    char input[256];
    while (1) {
      printf("[%s] %s > ", roomname, username);
      fflush(stdout);
      if (fgets(input, sizeof(input), stdin) == NULL) {
        break;
      }
      if (strlen(input) == 0) {
        continue;
      }
      
      int len = strlen(input);
      if (input[len - 1] == '\n') {
        input[len - 1] = '\0';
      }
      
      if (strcmp(input, "quit") == 0) {
        break;
      }
      
      snprintf(command, sizeof(command), "ls %s | while read pipe; do echo '[%s] %s: %s' > %s/$pipe & done", room_path, roomname, username, input, room_path);
      system(command);
    }
  
    wait(NULL);
    return SUCCESS;
  }

  // skeleton command implementation
  if (strcmp(command->name, "skeleton") == 0) {
    if (command->arg_count < 3) {
      printf("-%s: %s: %s\n", sysname, command->name, strerror(errno));
      return SUCCESS;
    }

    char *langname = command->args[1];
    char *projectname = command->args[2];
    char cmd[512], filepath[256];
    FILE *f;

    if (strcmp(langname, "c") == 0) {
      // create C 
      snprintf(cmd, sizeof(cmd), "mkdir -p %s/src %s/include", projectname, projectname);
      system(cmd);

      snprintf(filepath, sizeof(filepath), "%s/include/%s.h", projectname, projectname);
      f = fopen(filepath, "w");
      fprintf(f, "#ifndef %s_H\n#define %s_H\n\nvoid hello();\n\n#endif\n", projectname, projectname);
      fclose(f);

      snprintf(filepath, sizeof(filepath), "%s/src/main.c", projectname);
      f = fopen(filepath, "w");
      fprintf(f, "#include \"%s/%s.h\"\n#include <stdio.h>\n\nvoid hello() {\n    printf(\"Hello from C!\\n\");\n}\n\nint main() {\n    hello();\n    return 0;\n}\n", projectname, projectname);
      fclose(f);

      snprintf(filepath, sizeof(filepath), "%s/Makefile", projectname);
      f = fopen(filepath, "w");
      fprintf(f, "CC=gcc\nCFLAGS=-I./include -Wall\nOBJS=src/main.c\nTARGET=%s\n\nall: $(TARGET)\n\n$(TARGET): $(OBJS)\n\t$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)\n\nclean:\n\trm -f $(TARGET)\n", projectname);
      fclose(f);

      snprintf(filepath, sizeof(filepath), "%s/README.md", projectname);
      f = fopen(filepath, "w");
      fprintf(f, "# %s\n\nA simple C project.\n\n## Building\n\n```\nmake\n```\n", projectname);
      fclose(f);

      snprintf(filepath, sizeof(filepath), "%s/.gitignore", projectname);
      f = fopen(filepath, "w");
      fprintf(f, "%s\n*.o\n*.a\n*.so\n.build/\nbuild/\n", projectname);
      fclose(f);

    } else if (strcmp(langname, "cpp") == 0) {
      // create C++
      snprintf(cmd, sizeof(cmd), "mkdir -p %s/src %s/include", projectname, projectname);
      system(cmd);

      snprintf(filepath, sizeof(filepath), "%s/include/%s.h", projectname, projectname);
      f = fopen(filepath, "w");
      fprintf(f, "#ifndef %s_H\n#define %s_H\n\nclass App {\npublic:\n    void run();\n};\n\n#endif\n", projectname, projectname);
      fclose(f);

      snprintf(filepath, sizeof(filepath), "%s/src/main.cpp", projectname);
      f = fopen(filepath, "w");
      fprintf(f, "#include \"%s/%s.h\"\n#include <iostream>\n\nvoid App::run() {\n    std::cout << \"Hello from C++!\" << std::endl;\n}\n\nint main() {\n    App app;\n    app.run();\n    return 0;\n}\n", projectname, projectname);
      fclose(f);

      snprintf(filepath, sizeof(filepath), "%s/CMakeLists.txt", projectname);
      f = fopen(filepath, "w");
      fprintf(f, "cmake_minimum_required(VERSION 3.10)\nproject(%s)\n\nset(CMAKE_CXX_STANDARD 17)\n\ninclude_directories(include)\n\nadd_executable(%s src/main.cpp)\n", projectname, projectname);
      fclose(f);

      snprintf(filepath, sizeof(filepath), "%s/README.md", projectname);
      f = fopen(filepath, "w");
      fprintf(f, "# %s\n\nA simple C++ project.\n\n## Building\n\n```\nmkdir build\ncd build\ncmake ..\nmake\n```\n", projectname);
      fclose(f);

      snprintf(filepath, sizeof(filepath), "%s/.gitignore", projectname);
      f = fopen(filepath, "w");
      fprintf(f, "%s\n*.o\n*.a\n*.so\ncmake-build-debug/\nbuild/\n.idea/\n", projectname);
      fclose(f);

    } else if (strcmp(langname, "python") == 0) {
      // create Python
      snprintf(cmd, sizeof(cmd), "mkdir -p %s/src/%s %s/tests", projectname, projectname, projectname);
      system(cmd);

      snprintf(filepath, sizeof(filepath), "%s/src/%s/__init__.py", projectname, projectname);
      f = fopen(filepath, "w");
      fprintf(f, "\"\"\"Package initialization.\"\"\"\n__version__ = \"0.1.0\"\n");
      fclose(f);

      snprintf(filepath, sizeof(filepath), "%s/src/%s/__main__.py", projectname, projectname);
      f = fopen(filepath, "w");
      fprintf(f, "from .cli import main\n\nif __name__ == \"__main__\":\n    main()\n");
      fclose(f);

      snprintf(filepath, sizeof(filepath), "%s/src/%s/cli.py", projectname, projectname);
      f = fopen(filepath, "w");
      fprintf(f, "def main():\n    \"\"\"Main entry point.\"\"\"\n    print(\"Hello from Python!\")\n");
      fclose(f);

      snprintf(filepath, sizeof(filepath), "%s/tests/test_basic.py", projectname);
      f = fopen(filepath, "w");
      fprintf(f, "def test_import():\n    \"\"\"Test that module can be imported.\"\"\"\n    import sys\n    import os\n    sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'src'))\n    import %s\n    assert %s.__version__\n", projectname, projectname);
      fclose(f);

      snprintf(filepath, sizeof(filepath), "%s/pyproject.toml", projectname);
      f = fopen(filepath, "w");
      fprintf(f, "[build-system]\nrequires = [\"setuptools>=45\", \"wheel\"]\nbuild-backend = \"setuptools.build_meta\"\n\n[project]\nname = \"%s\"\nversion = \"0.1.0\"\ndescription = \"A simple Python project\"\n", projectname);
      fclose(f);

      snprintf(filepath, sizeof(filepath), "%s/README.md", projectname);
      f = fopen(filepath, "w");
      fprintf(f, "# %s\n\nA simple Python project.\n\n## Running\n\n```\npython -m %s\n```\n\n## Testing\n\n```\npytest tests/\n```\n", projectname, projectname);
      fclose(f);

      snprintf(filepath, sizeof(filepath), "%s/.gitignore", projectname);
      f = fopen(filepath, "w");
      fprintf(f, "__pycache__/\n*.py[cod]\n*$py.class\n*.so\n.Python\nbuild/\ndevelop-eggs/\ndist/\ndownloads/\neggs/\n.eggs/\nlib/\nlib64/\nparts/\nsdist/\nvar/\nwheels/\n.egg-info/\n.installed.cfg\n*.egg\n.pytest_cache/\n.coverage\nhtmlcov/\n");
      fclose(f);

    } else if (strcmp(langname, "java") == 0) {
      // create Java
      snprintf(cmd, sizeof(cmd), "mkdir -p %s/src/main/java/com/example/%s %s/src/test/java/com/example/%s", projectname, projectname, projectname, projectname);
      system(cmd);

      snprintf(filepath, sizeof(filepath), "%s/src/main/java/com/example/%s/App.java", projectname, projectname);
      f = fopen(filepath, "w");
      fprintf(f, "package com.example.%s;\n\npublic class App {\n    public static void main(String[] args) {\n        System.out.println(\"Hello from Java!\");\n    }\n}\n", projectname);
      fclose(f);

      snprintf(filepath, sizeof(filepath), "%s/src/test/java/com/example/%s/AppTest.java", projectname, projectname);
      f = fopen(filepath, "w");
      fprintf(f, "package com.example.%s;\n\nimport static org.junit.Assert.*;\nimport org.junit.Test;\n\npublic class AppTest {\n    @Test\n    public void testApp() {\n        assertTrue(true);\n    }\n}\n", projectname);
      fclose(f);

      snprintf(filepath, sizeof(filepath), "%s/pom.xml", projectname);
      f = fopen(filepath, "w");
      fprintf(f, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<project>\n    <modelVersion>4.0.0</modelVersion>\n    <groupId>com.example</groupId>\n    <artifactId>%s</artifactId>\n    <version>1.0</version>\n    <dependencies>\n        <dependency>\n            <groupId>junit</groupId>\n            <artifactId>junit</artifactId>\n            <version>4.13.2</version>\n            <scope>test</scope>\n        </dependency>\n    </dependencies>\n</project>\n", projectname);
      fclose(f);

      snprintf(filepath, sizeof(filepath), "%s/README.md", projectname);
      f = fopen(filepath, "w");
      fprintf(f, "# %s\n\nA simple Java project.\n\n## Building\n\n```\nmvn clean package\n```\n\n## Running\n\n```\njava -cp target/classes com.example.%s.App\n```\n", projectname, projectname);
      fclose(f);

      snprintf(filepath, sizeof(filepath), "%s/.gitignore", projectname);
      f = fopen(filepath, "w");
      fprintf(f, "target/\n.classpath\n.project\n.settings/\n*.class\n*.jar\n.idea/\n*.iml\n");
      fclose(f);

    } else {
      printf("-%s: %s: %s\n", sysname, command->name, strerror(errno));
    }

    return SUCCESS;
  }

  pid_t pid = fork();
  if (pid == 0) // child
  {
    // pipe
    if (command->next) {
      int pipe_file_d[2];
      pipe(pipe_file_d);
      pid_t pid2 = fork();
      
      if (pid2 != 0) {
        close(pipe_file_d[0]);
        dup2(pipe_file_d[1], STDOUT_FILENO);
        close(pipe_file_d[1]);
      } 
      else {
        close(pipe_file_d[1]);
        dup2(pipe_file_d[0], STDIN_FILENO);
        close(pipe_file_d[0]);
        command = command->next;
      }
    }

    /// This shows how to do exec with environ (but is not available on MacOs)
    // extern char** environ; // environment variables
    // execvpe(command->name, command->args, environ); // exec+args+path+environ

    /// This shows how to do exec with auto-path resolve
    // add a NULL argument to the end of args, and the name to the beginning
    // as required by exec

    // TODO: do your own exec with path resolving using execv()
    // do so by replacing the execvp call below
    
    // Handle <
    if (command->redirects[0]) {
      freopen(command->redirects[0], "r", stdin);
    }

    // Handle >
    if (command->redirects[1]) {
      freopen(command->redirects[1], "w", stdout);
    }

    // Handle >>
    if (command->redirects[2]) {
      freopen(command->redirects[2], "a", stdout);
    }

    char *path = find_executable(command->name);
    execv(path, command->args); // exec+args+path
    printf("-%s: %s: command not found\n", sysname, command->name);
    exit(127);
  } else {
    if (command->background) {
      return SUCCESS;
    }
    else {
      wait(0);
      return SUCCESS;
    }
  }
}

int main() {
  while (1) {
    struct command_t *command =
        (struct command_t *)malloc(sizeof(struct command_t));
    memset(command, 0, sizeof(struct command_t)); // set all bytes to 0

    int code;
    code = prompt(command);
    if (code == EXIT)
      break;

    code = process_command(command);
    if (code == EXIT)
      break;

    free_command(command);
  }

  printf("\n");
  return 0;
}
