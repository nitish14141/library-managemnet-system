# Library Management System (C, Windows GUI)

A medium-level desktop project written in C using the native Win32 API, with a dark navy background and colourful action buttons. It uses dynamic memory allocation (`malloc`/`realloc`/`free` family; this project uses `realloc` and `free`) and binary file handling (`fopen`, `fread`, `fwrite`, `fclose`).

## Features
- Add, update and delete book records
- Search by book title, author, or ID
- Issue and return copies
- Track total and available copies
- Persistent storage in `books.dat`
- Dynamic array that grows as records are added
- Input validation and duplicate-ID prevention

## Requirements
- Windows 10/11
- GCC from MinGW-w64 (MSYS2 recommended)
- Visual Studio Code (optional editor)

## Folder structure
```
LibraryManagementSystem/
├── main.c
├── build.bat
├── run.bat
├── README.md
└── books.dat       (created automatically after the first successful save)
```

## Build with GCC
Open a terminal in this folder and run:
```
gcc main.c -o LibraryManagementSystem.exe -mwindows -lcomdlg32
```
If your GCC installation is configured correctly, this creates `LibraryManagementSystem.exe`.

Alternatively, double-click `build.bat`.

## Run
```
.\LibraryManagementSystem.exe
```
or double-click `run.bat` after building.

## Using the application
1. Enter a unique positive Book ID, title, author, year, and total copies.
2. Click **Add Book** to create a record.
3. To update/delete/issue/return, enter a Book ID. For updating, enter the existing ID and the new details in the form.
4. Double-click a row to load the book details into the form.
5. Search using title, author, or ID; click **Show All** to reset the list.
6. The app saves changes to `books.dat` in the current working directory.

## Important notes
- This is a learning project, not a production library system.
- The binary data file stores C struct records. Keep the same program version/compiler layout when reusing the file. Do not manually edit `books.dat`.
- Deleting is blocked while any copy is issued. Updating the total copies preserves the number currently issued.
- This simplified project does not track individual borrowers, due dates, or fines.
