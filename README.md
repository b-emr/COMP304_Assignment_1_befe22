# COMP304_Assignment_1_befe22

## Github Repo Link: https://github.com/b-emr/COMP304_Assignment_1_befe22.git

## Part 1:
In that part we need to implement our execvp() command using execv() function. For that purpose we need to search our environment and find the executable command which name is given in the shell. So I implement *find_executable()* function for search and find executable in the PATH variables.

And also we need to implement background execution. For that purpose we need to change parent's *wait(0)* command. We need to take it inside a if closure.

## Part 2:
In that part we need to implement I/O redirection and for that purpose I use *freopen()* function. It reassign file stream to a different file.

And also we need to implement piping operation. For that purpose I use *pipe()* function. It takes a file descriptor and I use dup2() function to redirect that file descriptor to appropriate destination.

## Part 3:
In that part we need to implement cut command that takes 2 argument -d and -f which are the delimiter and the fields indexes. For that purpose I use standart C string libraries.

And in that part we also need to implement chatroom command which has named pipes in it. I test this command with 2 seperate terminals and communicate 2 terminals each other. But message input prompt has some bugs in it. I cannot solve it.

And finally I implement *skeleton* command. It takes 2 argument first one is the language name second one is project name and it creates a project backbone with selected language.



```text
Bünyamin Emre Efe
83827
College of Engineering
```