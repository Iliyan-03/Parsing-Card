# VCard Parsing

This project is a Python GUI application with a C backend for managing digital contact cards (vCards). It helps users organize, view, edit, and search contacts stored in .vcf or .vcard files.

## Features
The system has two main parts:

### 1. Python GUI Frontend – A user-friendly interface with different screens:

Login – Connect to a MySQL database

Main View – Lists all contact cards

Detail View – Shows full contact info

Editor – Create or modify contacts

Database Queries – Search for contacts (e.g., "Show June birthdays")

### 2. C Backend (libvcparser.so) 

Handles the actual reading, writing, and validation of vCard files. 

It ensures the contacts follow the correct format before saving.

## How It Works
The Python part manages the display, user input, and database.

The C part does the heavy lifting of parsing and validating vCards.

When you edit a contact, the system updates both the file and the database automatically.

## Setup
Install Python (3.8+) and MySQL.

Compile the C library by 
gcc -shared -fPIC -o libvcparser.so vcparser.c

Run gui
python main.py

Why This Project?
Easy to use – Clean interface for managing contacts.
Efficient – C backend makes file operations fast.
Reliable – Checks for errors before saving changes.
Great for organizing personal contacts or testing database/GUI programming! 

