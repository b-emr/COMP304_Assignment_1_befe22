# COMP304_Assignment_1_befe22

# Part 3: Custom Command
## *skeleton*
*skeleton* is a command that creates project skeletons for many languages. 

It currently supports 4 languages:
- Python (python)
- Java (java)
- C (c)s
- C++ (cpp)

 Usage
You can use that command as follows:
- *"skeleton language_name project_name"*

## Python
For Python it creates a project skeleton as follows:

```text
project_name/
 ├── src/
 │    └── project_name/
 │         ├── __init__.py
 │         ├── __main__.py
 │         └── cli.py
 ├── tests/
 │    └── test_basic.py
 ├── pyproject.toml
 ├── README.md
 └── .gitignore
```
## Java
For Java it creates a project skeleton as follows:

```text
project_name/
 ├── pom.xml
 ├── src/
 │    ├── main/
 │    │    └── java/
 │    │         └── com/
 │    │              └── example/
 │    │                   └── project_name/
 │    │                        └── App.java
 │    └── test/
 │         └── java/
 │              └── com/
 │                   └── example/
 │                        └── project_name/
 │                             └── AppTest.java
 ├── README.md
 └── .gitignore
 ```

 ## C
 For C it creates a project skeleton as follows:

```text
 project_name/
 ├── src/
 │    └── main.c
 ├── include/
 │    └── project_name.h
 ├── Makefile
 ├── README.md
 └── .gitignore
 ```

 ## C++
  For C++ it creates a project skeleton as follows:

```text
 project_name/
 ├── src/
 │    └── main.cpp
 ├── include/
 │    └── project_name.h
 ├── CMakeLists.txt
 ├── README.md
 └── .gitignore
 ```
