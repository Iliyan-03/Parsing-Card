import os
import ctypes
import mysql.connector
from mysql.connector import Error
from datetime import datetime
from asciimatics.screen import Screen
from asciimatics.scene import Scene
from asciimatics.widgets import Frame, ListBox, Layout, Divider, Text, Button, Widget, Label, PopUpDialog, TextBox
from asciimatics.exceptions import NextScene, StopApplication, ResizeScreenError

cursor = None
conn = None

def vcard_to_mysql_datetime(date_str, time_str, is_utc=False):
    if not date_str or not time_str:
        return None
    
    try:
        if len(date_str) == 8:
            year = int(date_str[0:4])
            month = int(date_str[4:6])
            day = int(date_str[6:8])
        else:
            return None

        if len(time_str) >= 6:
            hour = int(time_str[0:2])
            minute = int(time_str[2:4])
            second = int(time_str[4:6])
        else:
            return None

        return datetime(year, month, day, hour, minute, second)
    
    except (ValueError, IndexError):
        return None

libvcparser = ctypes.CDLL('./libvcparser.so')

class Node(ctypes.Structure):
    pass

Node._fields_ = [
    ("data", ctypes.c_void_p),
    ("previous", ctypes.POINTER(Node)),
    ("next", ctypes.POINTER(Node))
]

class List(ctypes.Structure):
    _fields_ = [
        ("head", ctypes.POINTER(Node)),
        ("tail", ctypes.POINTER(Node)),
        ("length", ctypes.c_int),
        ("deleteData", ctypes.c_void_p),
        ("compare", ctypes.c_void_p),
        ("printData", ctypes.c_void_p)
    ]

class DateTime(ctypes.Structure):
    _fields_ = [
        ("UTC", ctypes.c_bool),
        ("isText", ctypes.c_bool),
        ("date", ctypes.c_char_p),
        ("time", ctypes.c_char_p),
        ("text", ctypes.c_char_p)
    ]

class Parameter(ctypes.Structure):
    _fields_ = [
        ("name", ctypes.c_char_p),
        ("value", ctypes.c_char_p)
    ]

class Property(ctypes.Structure):
    _fields_ = [
        ("name", ctypes.c_char_p),
        ("group", ctypes.c_char_p),
        ("parameters", ctypes.POINTER(List)),
        ("values", ctypes.POINTER(List))
    ]

class Card(ctypes.Structure):
    _fields_ = [
        ("fn", ctypes.POINTER(Property)),
        ("optionalProperties", ctypes.POINTER(List)),
        ("birthday", ctypes.POINTER(DateTime)),
        ("anniversary", ctypes.POINTER(DateTime))
    ]

libvcparser.createCard.restype = ctypes.c_int
libvcparser.createCard.argtypes = [ctypes.c_char_p, ctypes.POINTER(ctypes.POINTER(Card))]

libvcparser.validateCard.restype = ctypes.c_int
libvcparser.validateCard.argtypes = [ctypes.POINTER(Card)]

libvcparser.writeCard.restype = ctypes.c_int
libvcparser.writeCard.argtypes = [ctypes.c_char_p, ctypes.POINTER(Card)]

libvcparser.deleteCard.restype = None
libvcparser.deleteCard.argtypes = [ctypes.POINTER(Card)]

def create_tables():
    global conn, cursor
    try:
        cursor.execute("""
            CREATE TABLE IF NOT EXISTS FILE (
                file_id INT AUTO_INCREMENT PRIMARY KEY,
                file_name VARCHAR(60) NOT NULL,
                last_modified DATETIME,
                creation_time DATETIME NOT NULL
            ) ENGINE=InnoDB
        """)
        cursor.execute("""
            CREATE TABLE IF NOT EXISTS CONTACT (
                contact_id INT AUTO_INCREMENT PRIMARY KEY,
                name VARCHAR(256) NOT NULL,
                birthday DATETIME,
                anniversary DATETIME,
                file_id INT NOT NULL,
                FOREIGN KEY (file_id) REFERENCES FILE(file_id) ON DELETE CASCADE
            ) ENGINE=InnoDB
        """)
        conn.commit()
        
        cursor.execute("SHOW TABLES")
        tables = cursor.fetchall()
        
    except Error as e:
        raise  

class LoginView(Frame):
    def __init__(self, screen):
        super(LoginView, self).__init__(screen,
                                        int(screen.height * 1),
                                        int(screen.width * 1),
                                        hover_focus=True,
                                        can_scroll=False,
                                        title="Login")

        layout = Layout([100], fill_frame=True)
        self.add_layout(layout)
        layout.add_widget(Text("Username:", "username"))
        layout.add_widget(Text("Password:", "password"))
        layout.add_widget(Text("Database Name:", "db_name"))
        layout.add_widget(Divider())
        layout2 = Layout([1, 1])
        self.add_layout(layout2)
        layout2.add_widget(Button("OK", self._login), 0)
        layout2.add_widget(Button("Cancel", self._cancel), 1)
        self.fix()

    def _login(self):
        self.save()
        username = self.data["username"]
        password = self.data["password"]
        db_name = self.data["db_name"]

        global conn
        global cursor

        try:
            conn = mysql.connector.connect(
                host="dursley.socs.uoguelph.ca",
                user=username,
                password=password,
                database=db_name
            )            
            cursor = conn.cursor()

            conn.autocommit = True

            create_tables()

            main_view = MainView(self._screen)
            main_view._update_card_list()
            
            if conn and conn.is_connected():
                raise NextScene("Main")
            else:
                self._scene.add_effect(
                    PopUpDialog(self._screen, "Database connection could not be verified", ["OK"], has_shadow=True)
                )
        except Error as e:
            self._scene.add_effect(
                PopUpDialog(self._screen, f"Login failed: {e}", ["OK"], has_shadow=True))

    def _cancel(self):
        main_view = MainView(self._screen)
        raise NextScene("Main")

class MainView(Frame):
    def __init__(self, screen):
        global conn, cursor
        
        super(MainView, self).__init__(screen,
                                     int(screen.height * 1),
                                     int(screen.width * 1),
                                     hover_focus=True,
                                     can_scroll=False,
                                     title="vCard List")

        self._card_list = ListBox(
            Widget.FILL_FRAME,
            options=[],
            name="card_list",
            on_select=self._edit_card
        )

        layout = Layout([100], fill_frame=True)
        self.add_layout(layout)
        layout.add_widget(self._card_list)
        layout.add_widget(Divider())

        layout2 = Layout([1, 1, 1, 1])
        self.add_layout(layout2)
        layout2.add_widget(Button("Create", self._create_card), 0)
        layout2.add_widget(Button("Edit", lambda: self._edit_card(edit_mode=True)), 1)
        layout2.add_widget(Button("DB Queries", self._db_queries), 2)
        layout2.add_widget(Button("Exit", self._exit), 3) 

        self.fix()
        self._update_card_list()
        self.add_scene = None

      
    def _update_card_list(self):
        self._card_list.options = []
        card_dir = "./cards"
                
        if not os.path.exists(card_dir):
            return

        if conn is None:
            for filename in os.listdir(card_dir):
                if filename.endswith((".vcf", ".vcard")):
                    self._card_list.options.append((filename, filename))
            return

        try:  
            cursor.execute("SELECT file_id, file_name, last_modified FROM FILE")
            existing_files = {row[1]: (row[0], row[2]) for row in cursor.fetchall()}
            
            while cursor.nextset():
                pass
            
            processed_files = 0
            updated_files = 0
            for filename in os.listdir(card_dir):
                if filename.endswith((".vcf", ".vcard")):
                    file_path = os.path.join(card_dir, filename)
                    file_mtime = os.path.getmtime(file_path)
                    last_modified = datetime.fromtimestamp(file_mtime)
                    creation_time = datetime.now()

                    card_ptr = ctypes.POINTER(Card)()
                    result = libvcparser.createCard(file_path.encode(), ctypes.byref(card_ptr))
                    
                    if result == 0:
                        try:
                            validation_result = libvcparser.validateCard(card_ptr)
                            
                            if validation_result == 0:
                                self._card_list.options.append((filename, filename))
                                processed_files += 1

                                contact_name = "Unknown"
                                if card_ptr.contents.fn:
                                    fn_property = card_ptr.contents.fn.contents
                                    if fn_property.values:
                                        value_list_ptr = fn_property.values
                                        if value_list_ptr.contents.head:
                                            current_node = value_list_ptr.contents.head
                                            while current_node:
                                                value_ptr = ctypes.cast(current_node.contents.data, ctypes.POINTER(ctypes.c_char))
                                                if value_ptr:
                                                    try:
                                                        contact_name = ctypes.string_at(value_ptr).decode('utf-8', errors='replace')
                                                        break
                                                    except Exception:
                                                        contact_name = "Unknown (Decode Error)"
                                                current_node = current_node.contents.next

                                birthday = None
                                if card_ptr.contents.birthday:
                                    bday = card_ptr.contents.birthday.contents
                                    if not bday.isText and bday.date and bday.time:
                                        date_str = ctypes.string_at(bday.date).decode('utf-8')
                                        time_str = ctypes.string_at(bday.time).decode('utf-8')
                                        birthday = vcard_to_mysql_datetime(date_str, time_str, bday.UTC)
                                        birthday_display = f"{date_str} {time_str}"
                                        if bday.UTC:
                                            birthday_display += " (UTC)"

                                anniversary = None
                                if card_ptr.contents.anniversary:
                                    anniv = card_ptr.contents.anniversary.contents
                                    if not anniv.isText and anniv.date and anniv.time:
                                        date_str = ctypes.string_at(anniv.date).decode('utf-8')
                                        time_str = ctypes.string_at(anniv.time).decode('utf-8')
                                        anniversary = vcard_to_mysql_datetime(date_str, time_str, anniv.UTC)
                                        anniversary_display = f"{date_str} {time_str}"
                                        if anniv.UTC:
                                            anniversary_display += " (UTC)"


                                if filename in existing_files:
                                    file_id, db_mtime = existing_files[filename]
                                    if db_mtime.timestamp() < file_mtime:
                                        cursor.execute("""
                                            UPDATE FILE 
                                            SET last_modified = %s 
                                            WHERE file_id = %s
                                        """, (last_modified, file_id))
                                        
                                        cursor.execute("""
                                            UPDATE CONTACT 
                                            SET name = %s, birthday = %s, anniversary = %s 
                                            WHERE file_id = %s
                                        """, (contact_name, birthday, anniversary, file_id))
                                        
                                        updated_files += 1
                                else:
                                    cursor.execute("""
                                        INSERT INTO FILE (file_name, last_modified, creation_time)
                                        VALUES (%s, %s, %s)
                                    """, (filename, last_modified, creation_time))
                                    file_id = cursor.lastrowid
                                    cursor.execute("""
                                        INSERT INTO CONTACT (name, birthday, anniversary, file_id)
                                        VALUES (%s, %s, %s, %s)
                                    """, (contact_name, birthday, anniversary, file_id))
                                    
                        finally:
                            libvcparser.deleteCard(card_ptr)

            conn.commit()
                        
            cursor.execute("SELECT COUNT(*) FROM CONTACT")
            total_contacts = cursor.fetchone()[0]
            cursor.execute("SELECT COUNT(*) FROM FILE")
            total_files = cursor.fetchone()[0]
            
        except mysql.connector.Error as err:
            if conn:
                conn.rollback()
        finally:
            if 'cursor' in locals():
                cursor.close()

    def _create_card(self):
        raise NextScene("CreateCard")

    def _edit_card(self, edit_mode=False):
            self.save()
            if self._card_list.value:
                file_name = self._card_list.value
                card_ptr = ctypes.POINTER(Card)()
                file_path = os.path.join("./cards", file_name)

                result = libvcparser.createCard(file_path.encode(), ctypes.byref(card_ptr))
                if result == 0:
                    try:
                        contact_name = "Unknown"
                        if card_ptr.contents.fn:
                            fn_property = card_ptr.contents.fn.contents
                            if fn_property.values:
                                value_list_ptr = fn_property.values
                                if value_list_ptr.contents.head:
                                    current_node = ctypes.cast(value_list_ptr.contents.head, ctypes.POINTER(Node))
                                    while current_node:
                                        value_ptr = ctypes.cast(current_node.contents.data, ctypes.POINTER(ctypes.c_char))
                                        if value_ptr:
                                            try:
                                                contact_name = ctypes.string_at(value_ptr).decode('utf-8', errors='replace')
                                                break
                                            except Exception:
                                                contact_name = "Unknown (Decode Error)"
                                        current_node = current_node.contents.next

                        birthday = ""
                        if card_ptr.contents.birthday:
                            birthday = card_ptr.contents.birthday.contents
                            if birthday.text:
                                try:
                                    birthday = ctypes.string_at(birthday.text).decode('utf-8', errors='replace').strip()
                                except Exception:
                                    birthday = "Invalid Date"
                            else:
                                birthday_date = "No Date"
                                birthday_time = "No Time"
                                is_utc = False

                                if birthday.date:
                                    try:
                                        birthday_date = ctypes.string_at(birthday.date).decode('utf-8', errors='replace')
                                    except Exception:
                                        birthday_date = "Invalid Date"

                                if birthday.time:
                                    try:
                                        birthday_time = ctypes.string_at(birthday.time).decode('utf-8', errors='replace')
                                        is_utc = birthday.UTC or birthday_time.endswith('Z')
                                        if birthday_time.endswith('Z'):
                                            birthday_time = birthday_time[:-1]
                                    except Exception:
                                        birthday_time = "Invalid Time"

                                utc_str = " (UTC)" if is_utc else ""
                                birthday = f"Date: {birthday_date} Time: {birthday_time}{utc_str}"

                        anniversary = ""
                        if card_ptr.contents.anniversary:
                            anniversary = card_ptr.contents.anniversary.contents
                            if anniversary.text:
                                try:
                                    anniversary = ctypes.string_at(anniversary.text).decode('utf-8', errors='replace').strip()
                                except Exception:
                                    anniversary = "Invalid Date"
                            else:
                                anniversary_date = "No Date"
                                anniversary_time = "No Time"
                                is_utc = False

                                if anniversary.date:
                                    try:
                                        anniversary_date = ctypes.string_at(anniversary.date).decode('utf-8', errors='replace')
                                    except Exception:
                                        anniversary_date = "Invalid Date"

                                if anniversary.time:
                                    try:
                                        anniversary_time = ctypes.string_at(anniversary.time).decode('utf-8', errors='replace')
                                        is_utc = anniversary.UTC or anniversary_time.endswith('Z')
                                        if anniversary_time.endswith('Z'):
                                            anniversary_time = anniversary_time[:-1]
                                    except Exception:
                                        anniversary_time = "Invalid Time"

                                utc_str = " (UTC)" if is_utc else ""
                                anniversary = f"Date: {anniversary_date} Time: {anniversary_time}{utc_str}"

                        other_properties = card_ptr.contents.optionalProperties.contents.length if card_ptr.contents.optionalProperties else 0

                    except AttributeError:
                        contact_name = "Unknown"
                        birthday = ""
                        anniversary = ""
                        other_properties = 0

                    libvcparser.deleteCard(card_ptr)

                    if edit_mode:
                        edit_card_view = EditCardView(self.screen, file_name, contact_name, birthday, anniversary, other_properties)
                        if self.add_scene:
                            self.remove_scene("EditCard")
                            self.add_scene(Scene([edit_card_view], -1, name="EditCard"))
                        raise NextScene("EditCard")
                    else:
                        card_detail_view = CardDetailView(self.screen, file_name, contact_name, birthday, anniversary, other_properties)
                        if self.add_scene:
                            self.remove_scene("CardDetail")
                            self.add_scene(Scene([card_detail_view], -1, name="CardDetail"))
                        raise NextScene("CardDetail")

    def _db_queries(self):
        raise NextScene("DBQueries")

    def _exit(self):
        raise StopApplication("User pressed exit")

class CardDetailView(Frame):
    def __init__(self, screen, file_name, contact_name, birthday="", anniversary="", other_properties=0):
        super(CardDetailView, self).__init__(screen,
                                             int(screen.height * 1),
                                             int(screen.width * 1),
                                             hover_focus=True,
                                             can_scroll=False,
                                             title="vCard details")

        
        layout = Layout([100], fill_frame=True)
        self.add_layout(layout)

        layout.add_widget(Label(f"File name: {file_name}"))
        layout.add_widget(Label(f"Contact: {contact_name}"))
        layout.add_widget(Label(f"Birthday: {birthday}"))
        layout.add_widget(Label(f"Anniversary: {anniversary}"))
        layout.add_widget(Label(f"Other properties: {other_properties}"))

        layout2 = Layout([1, 1])
        self.add_layout(layout2)
        layout2.add_widget(Button("OK", self._ok), 0)
        layout2.add_widget(Button("Cancel", self._cancel), 1)

        self.fix()

    def _ok(self):
        raise NextScene("Main")

    def _cancel(self):
        raise NextScene("Main")

class CreateCardView(Frame):
    def __init__(self, screen):
        super(CreateCardView, self).__init__(screen,
                                             int(screen.height * 1),
                                             int(screen.width * 1),
                                             hover_focus=True,
                                             can_scroll=False,
                                             title="Create vCard")
        self.debug_file = "vcard_debug.log"  
        open(self.debug_file, 'w').close()
        
        layout = Layout([100], fill_frame=True)
        self.add_layout(layout)
        layout.add_widget(Text("File Name:", "file_name"))
        layout.add_widget(Text("Contact Name:", "contact_name"))
        layout.add_widget(Divider())
        layout2 = Layout([1, 1])
        self.add_layout(layout2)
        layout2.add_widget(Button("OK", self._ok), 0)
        layout2.add_widget(Button("Cancel", self._cancel), 1)
        self.fix()

    def _log_debug(self, message):
        with open(self.debug_file, 'a') as f:
            timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
            f.write(f"[{timestamp}] {message}\n")

    def _ok(self):
        self.save()
        file_name = self.data["file_name"].strip()
        contact_name = self.data["contact_name"].strip()

        self._log_debug(f"Starting vCard creation with filename='{file_name}', contact='{contact_name}'")

        if not file_name:
            self._show_error("File name cannot be empty!")
            return
        if not contact_name:
            self._show_error("Contact name cannot be empty!")
            return

        if not file_name.lower().endswith(('.vcf', '.vcard')):
            file_name += '.vcf'

        cards_dir = "./cards"
        try:
            os.makedirs(cards_dir, exist_ok=True)
        except OSError as e:
            self._show_error(f"Cannot create cards directory: {e}")
            return

        file_path = os.path.join(cards_dir, file_name)
        
        if os.path.exists(file_path):
            self._show_error(f"File '{file_name}' already exists!")
            return

        vcard_content = f"""BEGIN:VCARD
    VERSION:4.0
    FN:{contact_name}
    END:VCARD
    """

        try:
            with open(file_path, 'w', encoding='utf-8', newline='\r\n') as f:
                f.write(vcard_content)

            if not os.path.exists(file_path):
                self._show_error("Failed to create vCard file!")
                return

            card_ptr = ctypes.POINTER(Card)()
            result = libvcparser.createCard(file_path.encode('utf-8'), ctypes.byref(card_ptr))
            
            if result != 0:
                self._show_error("Failed to parse the created vCard!")
                os.remove(file_path)
                return

            validation_result = libvcparser.validateCard(card_ptr)
            libvcparser.deleteCard(card_ptr)
            
            if validation_result != 0:
                self._show_error("Created card is invalid!")
                os.remove(file_path)
                return

            try:
                creation_time = datetime.now()
                cursor.execute("""
                    INSERT INTO FILE (file_name, last_modified, creation_time)
                    VALUES (%s, %s, %s)
                """, (file_name, creation_time, creation_time))
                
                file_id = cursor.lastrowid
                
                cursor.execute("""
                    INSERT INTO CONTACT (name, file_id)
                    VALUES (%s, %s)
                """, (contact_name, file_id))
                
                conn.commit()
                raise NextScene("Main")
                
            except Error as e:
                conn.rollback()
                self._show_error(f"Database error: {e}")
                os.remove(file_path)
                return

        except Exception as e:
            self._show_error(f"Error creating vCard: {str(e)}")
            if os.path.exists(file_path):
                os.remove(file_path)
            return

    def _cancel(self):
        self._log_debug("User cancelled vCard creation")
        raise NextScene("Main")

    def _show_error(self, message):
        self._log_debug(f"Showing error to user: {message}")
        self._scene.add_effect(
            PopUpDialog(self._screen, message, ["OK"], has_shadow=True)
        )

class EditCardView(Frame):
    def __init__(self, screen, file_name, contact_name, birthday="", anniversary="", other_properties=0):
        super(EditCardView, self).__init__(screen,
                                           int(screen.height * 1),
                                           int(screen.width * 1),
                                           hover_focus=True,
                                           can_scroll=False,
                                           title="Edit vCard")

        self._file_name = file_name
        self._contact_name = contact_name
        self.data = {"contact_name": contact_name}

        layout = Layout([100], fill_frame=True)
        self.add_layout(layout)

        layout.add_widget(Label(f"File name: {file_name}"))
        layout.add_widget(Label(f"Contact: {contact_name}"))
        layout.add_widget(Label(f"Birthday: {birthday}"))
        layout.add_widget(Label(f"Anniversary: {anniversary}"))
        layout.add_widget(Label(f"Other properties: {other_properties}"))

        layout.add_widget(Divider())
        layout.add_widget(Text("Edit Contact Name:", "contact_name"))

        layout2 = Layout([1, 1])
        self.add_layout(layout2)
        layout2.add_widget(Button("Save", self._save), 0)
        layout2.add_widget(Button("Cancel", self._cancel), 1)

        self.fix()

    def _save(self):
        self.save()
        new_contact_name = self.data["contact_name"].strip()

        if not new_contact_name:
            self._show_error("Contact name cannot be empty!")
            return

        card_ptr = ctypes.POINTER(Card)()
        file_path = os.path.join("./cards", self._file_name)

        try:
            result = libvcparser.createCard(file_path.encode(), ctypes.byref(card_ptr))
            if result != 0 or not card_ptr:
                self._show_error("Failed to load the vCard file!")
                return

            validation_result = libvcparser.validateCard(card_ptr)
            if validation_result != 0:
                self._show_error("Card is invalid before modification!")
                libvcparser.deleteCard(card_ptr)
                return

            if card_ptr.contents.fn:
                fn_property = card_ptr.contents.fn.contents
                if fn_property.values:
                    value_list_ptr = fn_property.values
                    if value_list_ptr.contents.head:
                        current_node = ctypes.cast(value_list_ptr.contents.head, ctypes.POINTER(Node))
                        if current_node:
                            value_ptr = ctypes.cast(current_node.contents.data, ctypes.POINTER(ctypes.c_char))
                            if value_ptr:
                                try:
                                    current_str = ctypes.string_at(value_ptr).decode('utf-8')
                                    current_length = len(current_str)
                                    
                                    if len(new_contact_name) == current_length:
                                        self._show_error(f"Name too long! Max {current_length} chars")
                                        libvcparser.deleteCard(card_ptr)
                                        return

                                    new_value = ctypes.create_string_buffer(new_contact_name.encode('utf-8'))
                                    ctypes.memmove(value_ptr, new_value, len(new_contact_name))
                                    
                                    value_ptr[len(new_contact_name)] = 0
                                except Exception as e:
                                    self._show_error(f"Memory error: {str(e)}")
                                    libvcparser.deleteCard(card_ptr)
                                    return

            validation_result = libvcparser.validateCard(card_ptr)
            if validation_result != 0:
                self._show_error("Invalid changes! Please correct the contact name.")
                libvcparser.deleteCard(card_ptr)
                return

            write_result = libvcparser.writeCard(file_path.encode(), card_ptr)
            
            libvcparser.deleteCard(card_ptr)
            card_ptr = None
        
            if write_result == 0:
                try:
                    file_path = os.path.join("./cards", self._file_name)
                    file_mtime = os.path.getmtime(file_path)
                    last_modified = datetime.fromtimestamp(file_mtime)
                    
                    cursor.execute("""
                        UPDATE FILE SET last_modified = %s 
                        WHERE file_name = %s
                    """, (last_modified, self._file_name))
                    
                    cursor.execute("""
                        UPDATE CONTACT SET name = %s 
                        WHERE file_id = (
                            SELECT file_id FROM FILE WHERE file_name = %s
                        )
                    """, (new_contact_name, self._file_name))
                    
                    conn.commit()
                except Error as e:
                    conn.rollback()
        
            raise NextScene("Main")

        except Exception as e:
            import traceback
            if 'card_ptr' in locals() and card_ptr:
                libvcparser.deleteCard(card_ptr)

    def _cancel(self):
        raise NextScene("Main")

    def _show_error(self, message):
        self._scene.add_effect(
            PopUpDialog(self._screen, message, ["OK"], has_shadow=True)
        )


class DBQueryView(Frame):
    def __init__(self, screen):
        super(DBQueryView, self).__init__(screen,
                                        int(screen.height * 1),
                                        int(screen.width * 1),
                                        hover_focus=True,
                                        can_scroll=False,
                                        title="DB Queries")

        self._results = TextBox(
            Widget.FILL_FRAME,
            label="Results:",
            name="results",
            readonly=True,
            line_wrap=True,
            as_string=True
        )
        self._results.disabled = True
        
        layout = Layout([100], fill_frame=True)
        self.add_layout(layout)
        layout.add_widget(self._results)
        layout.add_widget(Divider())
        
        layout2 = Layout([1, 1, 1])
        self.add_layout(layout2)
        layout2.add_widget(Button("All Contacts", self._display_all_contacts), 0)
        layout2.add_widget(Button("June Birthdays", self._find_june_birthdays), 1)
        layout2.add_widget(Button("Cancel", self._cancel), 2)

        self.fix()

    def _run_query(self, query, params=None):
        global conn
        
        if conn is None:
            self._show_error("No database connection available")
            return None
            
        cursor = None
        try:
            cursor = conn.cursor(dictionary=True, buffered=True)
            
            if params:
                cursor.execute(query, params)
            else:
                cursor.execute(query)
                
            results = cursor.fetchall()
            return results
            
        except mysql.connector.Error as err:
            self._show_error(f"Database error: {err}")
            return None
        finally:
            if cursor:
                cursor.close()

    def _display_all_contacts(self):
        global conn
        
        if conn is None:
            self._show_error("Not connected to database")
            return

        query = """
            SELECT 
                c.contact_id,
                c.name,
                c.birthday,
                c.anniversary,
                f.file_name
            FROM CONTACT c
            JOIN FILE f ON c.file_id = f.file_id
            ORDER BY c.name, f.file_name
        """
        
        results = self._run_query(query)
        if results is None:
            return
            
        output = []
        if not results:
            output.append("No contacts found in database")
        else:
            output.append(f"Found {len(results)} contacts:\n")
        
            id_width = max(len(str(row['contact_id'])) for row in results) + 2
            name_width = max(len(row['name']) for row in results) + 2
            bday_width = 12
            anni_width = 12  
            file_width = max(len(row['file_name']) for row in results) + 2
            
            header = (f"{'ID':<{id_width}} | "
                    f"{'Name':<{name_width}} | "
                    f"{'Birthday':<{bday_width}} | "
                    f"{'Anniversary':<{anni_width}} | "
                    f"{'File':<{file_width}}")
            output.append(header)
            output.append("-" * len(header))
            
            for row in results:
                birthday = row['birthday'].strftime('%Y-%m-%d') if row['birthday'] else "None"
                anniversary = row['anniversary'].strftime('%Y-%m-%d') if row['anniversary'] else "None"
                
                output.append(
                    f"{row['contact_id']:<{id_width}} | "
                    f"{row['name']:<{name_width}} | "
                    f"{birthday:<{bday_width}} | "
                    f"{anniversary:<{anni_width}} | "
                    f"{row['file_name']:<{file_width}}"
                )
        
        self._results.value = "\n".join(output)
        self.fix()

    def _find_june_birthdays(self):
        global conn
        
        if conn is None:
            self._show_error("Not connected to database")
            return

        query = """
            SELECT c.name, c.birthday
            FROM CONTACT c
            WHERE c.birthday IS NOT NULL 
            AND MONTH(c.birthday) = 6
            ORDER BY DAY(c.birthday), c.name
        """
        
        results = self._run_query(query)
        if results is None:
            return
            
        output = []
        if not results:
            output.append("No contacts with June birthdays found")
        else:
            name_width = max(len(row['name']) for row in results) + 2
            bday_width = 12 
            
            header = (f"{'Name':<{name_width}} | "
                    f"{'Birthday':<{bday_width}}")
            output.append(f"Found {len(results)} contacts with June birthdays:\n")
            output.append(header)
            output.append("-" * len(header))
            
            for row in results:
                birthday = row['birthday'].strftime('%Y-%m-%d') if row['birthday'] else "None"
                output.append(
                    f"{row['name']:<{name_width}} | "
                    f"{birthday:<{bday_width}} "
                )
        
        self._results.value = "\n".join(output)
        self.fix()

    def _cancel(self):
        raise NextScene("Main")

    def _show_error(self, message):
        self._scene.add_effect(
            PopUpDialog(self._screen, message, ["OK"], has_shadow=True)
        )

def demo(screen, last_scene=None):
    scenes = [
        Scene([LoginView(screen)], -1, name="Login"),
        Scene([MainView(screen)], -1, name="Main"),
        Scene([CreateCardView(screen)], -1, name="CreateCard"),
        Scene([DBQueryView(screen)], -1, name="DBQueries"),
    ]

    def add_scene(scene):
        scenes.append(scene)
        screen.force_update()

    def remove_scene(name):
        scenes[:] = [scene for scene in scenes if scene.name != name]
        screen.force_update()

    for scene in scenes:
        for effect in scene.effects:
            if isinstance(effect, MainView):
                effect.add_scene = add_scene
                effect.remove_scene = remove_scene

    screen.play(scenes, stop_on_resize=True, start_scene=last_scene)

if __name__ == "__main__":
    last_scene = None
    while True:
        try:
            last_scene = Screen.wrapper(demo, arguments=[last_scene])
            break
        except ResizeScreenError:
            pass