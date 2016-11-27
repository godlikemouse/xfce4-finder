#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#include <iostream>
#include <unordered_map>
#include <gtkmm.h>
#include <gdk/gdkkeysyms.h>
#include <dirent.h>
#include <spawn.h>
#include <glib/gi18n.h>
#include <locale.h>
#include <libxfce4util/libxfce4util.h>
#include <libxfce4ui/libxfce4ui.h>
#include <garcon/garcon.h>
#include <xfconf/xfconf.h>

#include "glade.h"
#include "default-css.h"
#include "custom-css.h"

Glib::RefPtr<Gtk::Builder> p_builder;
Glib::RefPtr<Gtk::Application> p_application;
Glib::RefPtr<Gtk::ListStore> p_list_store;
Gtk::Overlay*  p_autocomplete_overlay = nullptr;
Gtk::Window * p_main_window = nullptr;
Gtk::Entry * p_search_text = nullptr;
Gtk::Image * p_search_icon = nullptr;
Gtk::TreeView * p_autocomplete = nullptr;
Gtk::Button * p_search_application = nullptr;
Gtk::Image * p_search_arguments_icon = nullptr;
Gtk::Entry * p_search_arguments = nullptr;
Gtk::Box * p_arguments_layout = nullptr;
std::vector<Glib::ustring> directories;
Glib::ustring file_extension(".desktop");
Glib::ustring web_search("https://www.google.com/#safe=on&q=%s");
Glib::ustring theme("default");
int results_limit;
bool verbose = false;
Glib::ustring default_browser;
Glib::ustring default_browser_icon;

struct SearchEntry {
    Glib::ustring filename;
    Glib::ustring directory;
    std::size_t position;
    Glib::ustring name;
    Glib::ustring description;
    Glib::ustring exec;
    Glib::ustring icon;
};

struct ExecSpec {
    Glib::ustring cmd;
    std::vector<Glib::ustring> args;
};

class SearchEntryType {
    public:
    bool is_url;
    bool is_ftp;
    bool is_search;
    bool is_command;
    bool is_file;
    bool is_directory;
    bool is_application;
    Glib::ustring text;

    SearchEntryType(){
        is_url =
            is_ftp =
            is_search =
            is_application =
            is_command =
            is_file =
            is_directory = false;
    }

    SearchEntryType(Glib::ustring& s) : SearchEntryType() {
        text = s;
    }
};

class AutocompleteColumns : public Gtk::TreeModel::ColumnRecord {

    public:
    Gtk::TreeModelColumn< Glib::RefPtr<Gdk::Pixbuf> > icon;
    Gtk::TreeModelColumn<Glib::ustring> name;
    Gtk::TreeModelColumn<Glib::ustring> description;

    Gtk::TreeModelColumn<SearchEntry> data;

    AutocompleteColumns(){
        add(icon);
        add(name);
        add(description);
        add(data);
    }

};

const Glib::ustring HOME_DIRECTORY = getenv("HOME");
const AutocompleteColumns autocomplete_columns;

//method for loading configured channel settings and writing defaults
inline void load_channel_settings(){

    auto p_channel = xfconf_channel_get("xfce4-finder");

    //load directory settings
    GPtrArray * p_directories = nullptr;
    const char * APPLICATION_DIRECTORIES_PATH = "/application-directories";

    if(xfconf_channel_has_property(p_channel, APPLICATION_DIRECTORIES_PATH)){
        p_directories = xfconf_channel_get_arrayv(p_channel, APPLICATION_DIRECTORIES_PATH);
    }
    else {
        p_directories = g_ptr_array_new();

        std::vector<Glib::ustring> default_directories = {
            "/usr/share/applications",
            HOME_DIRECTORY + "/.local/share/applications"
        };

        for(auto iter = default_directories.begin();
            iter < default_directories.end();
            ++iter){

            GValue * p_value = new GValue();
            *p_value = G_VALUE_INIT;
            g_value_init(p_value, G_TYPE_STRING);
            g_value_set_string(p_value, (*iter).data());
            g_ptr_array_add(p_directories, p_value);
        }

        xfconf_channel_set_arrayv(p_channel, APPLICATION_DIRECTORIES_PATH, p_directories);
    }

    //set configured directories
    for(int i=0; i<p_directories->len; i++){
        Glib::ustring directory( g_value_get_string( (GValue*) g_ptr_array_index(p_directories, i) ) );
        directories.push_back(directory);
    }

    xfconf_array_free(p_directories);

    //get/set file extension
    const char * FILE_EXTENSION_PATH = "/file-extension";

    if(xfconf_channel_has_property(p_channel, FILE_EXTENSION_PATH)){
        file_extension = xfconf_channel_get_string(p_channel, FILE_EXTENSION_PATH, file_extension.data());
    }
    else{
        xfconf_channel_set_string(p_channel, FILE_EXTENSION_PATH, file_extension.data());
    }

    //get/set web search
    const char * WEB_SEARCH_PATH = "/web-search";

    if(xfconf_channel_has_property(p_channel, WEB_SEARCH_PATH)){
        web_search = xfconf_channel_get_string(p_channel, WEB_SEARCH_PATH, web_search.data());
    }
    else{
        xfconf_channel_set_string(p_channel, WEB_SEARCH_PATH, web_search.data());
    }

    //get/set theme
    const char * THEME_PATH = "/theme";

    if(xfconf_channel_has_property(p_channel, THEME_PATH)){
        theme = xfconf_channel_get_string(p_channel, THEME_PATH, theme.data());
    }
    else{
        xfconf_channel_set_string(p_channel, THEME_PATH, theme.data());
    }
}

//method for replacing all occurances of a string
inline Glib::ustring replace_all(const Glib::ustring& haystack, const Glib::ustring& needle, const Glib::ustring& replace) {

    Glib::ustring str = haystack;
    Glib::ustring::size_type pos;
    Glib::ustring::size_type offset = 0;
    while( (pos = str.find(needle, offset)) != Glib::ustring::npos ){
        offset = pos + replace.size();
        str = str.replace(pos, needle.size(), replace);
    }

    return str;
}

//method for beautifying the default css file
inline Glib::ustring pretty_print_css(const Glib::ustring& content){

    Glib::ustring c = content;
    c = replace_all(c, "}", "}\n\n");
    c = replace_all(c, "{", "{\n");
    c = replace_all(c, ";", ";\n");

    return c;
}

//method for splitting a string from a delimiter
inline std::vector<Glib::ustring> split(const Glib::ustring& value, const char delim){
    std::stringstream splitstream(value.data());
    std::string segment;
    std::vector<Glib::ustring> list;

    while(std::getline(splitstream, segment, delim)){
        list.push_back(Glib::ustring(segment));
    }

    return list;
}

//method for joining an array into a string by delimeter
inline Glib::ustring join(const std::vector<Glib::ustring>& value, const char delim){
    Glib::ustring str;
    int index = 0;
    for(auto iter = value.begin();
        iter < value.end();
        ++iter){

        str += *iter;

        if(++index < value.size())
            str += delim;
    }

    return str;
}

//method for left trimming a string
inline Glib::ustring ltrim(const Glib::ustring& s){
    Glib::ustring str = s;

    bool done = false;
    for(auto iter = str.begin();
        iter < str.end() && !done;
        ++iter){

        switch(*iter){
            case 32: //space
            case 10: //tab
            str.erase(iter);
            break;

            case 0: //null
            continue;

            default:
            done = true;
            break;
        }
    }

    return str;
}

//method for right trimming a string
inline Glib::ustring rtrim(const Glib::ustring& s){
    Glib::ustring str = s;

    bool done = false;
    for(auto iter = str.end();
        iter > str.begin() && !done;
        --iter){

        switch(*iter){
            case 32: //space
            case 10: //tab
            str.erase(iter);
            break;

            case 0: //null
            continue;

            default:
            done = true;
            break;
        }
    }

    return str;
}

//method for trimming a string
inline Glib::ustring trim(const Glib::ustring& s){
    return ltrim(rtrim(s));
}

//method for retrieving the current query text type
inline SearchEntryType get_search_entry_type(const Glib::ustring& text){

    SearchEntryType search_type;
    search_type.text = text;

    //get current selection
    Glib::RefPtr<Gtk::TreeSelection> selection = p_autocomplete->get_selection();
    Gtk::TreeModel::Row row = *(selection->get_selected());
    if(row){
        search_type.is_application = true;
        return search_type;
    }

    //detect web url
    //if text doesn't have any spaces and it contains at least one period
    search_type.text = trim(search_type.text);
    if(search_type.text.find(" ") == Glib::ustring::npos &&
       search_type.text.find(".") != Glib::ustring::npos){

        search_type.is_url = true;
    }

    if(search_type.is_url){
        if(search_type.text.find("ftp.") == 0)
            search_type.text = "ftp://" + search_type.text;

        if(search_type.text.find("ftp://") != Glib::ustring::npos)
            search_type.is_ftp = true;
    }

    //web search
    if(!row && search_type.text.find(" ") != Glib::ustring::npos){
        search_type.is_search = true;
    }

    //check file type
    struct stat file_info;
    Glib::ustring location = replace_all(search_type.text, "~", HOME_DIRECTORY);
    if(stat(location.data(), &file_info) != -1){
        switch(file_info.st_mode & S_IFMT){
            case S_IFDIR:
            search_type.is_directory = true;
            search_type.is_url = false;
            break;

            case S_IFREG:
            search_type.is_file = true;
            search_type.is_url = false;
            break;
        }
    }

    //fallback to command
    if(!search_type.is_url &&
       !search_type.is_ftp &&
       !search_type.is_search &&
       !search_type.is_directory &&
       !search_type.is_file &&
       !row &&
       text.size()){

        search_type.is_command = true;
    }

    return search_type;
}

//method for retrieving the current theme icon by name
inline Glib::RefPtr<Gdk::Pixbuf> get_theme_icon(const Glib::ustring& icon_name){

    Glib::RefPtr<Gdk::Pixbuf> icon;

    try{
        Glib::RefPtr<Gtk::IconTheme> theme = Gtk::IconTheme::get_default();
        int lookup_flags = Gtk::ICON_LOOKUP_USE_BUILTIN | Gtk::ICON_LOOKUP_FORCE_SIZE;
        icon = theme->load_icon(icon_name, lookup_flags);
    }
    catch(Glib::Error& ex){}

    return icon;
}

//method for launching an external process
inline void launch_process(const Glib::ustring cmd, std::vector<Glib::ustring> args){

    pid_t pid;
    char * argv[args.size() + 1];

    if(verbose)
        std::cout << "Launching: " << cmd;

    //convert vector to const char * array
    int index = 0;
    for(auto iter = args.begin();
        iter != args.end();
        ++iter){

        if(index > 0 && verbose)
            std::cout << " " << *iter;

        if( (*iter).size() )
            argv[index++] = const_cast<char*>((*iter).data());
    }
    argv[index] = NULL;

    if(verbose)
        std::cout << std::endl;

    posix_spawn(&pid, cmd.data(), NULL, NULL, argv, environ);
}

//method for sorting matches based on position
inline bool sort_position_matches(const SearchEntry& a, const SearchEntry& b){
    return a.position < b.position;
}

//method for sorting matches based on name if positions are the same
inline bool sort_name_matches(const SearchEntry& a, const SearchEntry& b){
    if(a.position == b.position)
        return a.name.compare(b.name) < 0;
    return a.position < b.position;
}

//method for sorting directory entries
inline bool sort_directory(const Glib::ustring& a, const Glib::ustring& b){
    return a.compare(b) < 0;
}

//method for verifying if a file exists
inline bool file_exists(const Glib::ustring filename) {
    if (FILE *file = fopen(filename.data(), "r")) {
        fclose(file);
        return true;
    }

    return false;
}

//method for writing to a file
inline bool write_file(const Glib::ustring filename, const Glib::ustring contents){
    if(FILE *file = fopen(filename.data(), "w")) {
        fputs(contents.data(), file);
        fclose(file);
        return true;
    }

    return false;
}

//method for executing a command and retrieving the output
Glib::ustring passthru(const Glib::ustring& cmd) {
    char buffer[128];
    Glib::ustring result = "";
    std::shared_ptr<FILE> pipe(popen(cmd.data(), "r"), pclose);
    if (!pipe) throw std::runtime_error("popen() failed!");
    while (!feof(pipe.get())) {
        if (fgets(buffer, 128, pipe.get()) != NULL)
            result += buffer;
    }
    return trim(result);
}

//method for creating an ExecSpec from an exec command string
inline ExecSpec parse_exec_path(const Glib::ustring& exec_path){

    //autocomplete parsing

    //replace ~ with home
    Glib::ustring search_arguments = replace_all(p_search_arguments->get_text(), "~", HOME_DIRECTORY);

    //break into command and arguments
    std::vector<Glib::ustring> exec_parts = split(exec_path, ' ');

    ExecSpec spec;

    for(auto iter = exec_parts.begin(); iter != exec_parts.end(); ++iter){
        if(iter == exec_parts.begin()){
            spec.cmd = *iter;
            spec.args.push_back(spec.cmd);
        }
        else{

            /*
            %f	A single file name, even if multiple files are selected. The system reading the desktop entry should recognize that the program in question cannot handle multiple file arguments, and it should should probably spawn and execute multiple copies of a program for each selected file if the program is not able to handle additional file arguments. If files are not on the local file system (i.e. are on HTTP or FTP locations), the files will be copied to the local file system and %f will be expanded to point at the temporary file. Used for programs that do not understand the URL syntax.
            %F	A list of files. Use for apps that can open several local files at once. Each file is passed as a separate argument to the executable program.
            %u	A single URL. Local files may either be passed as file: URLs or as file path.
            %U	A list of URLs. Each URL is passed as a separate argument to the executable program. Local files may either be passed as file: URLs or as file path.
            %i	The Icon key of the desktop entry expanded as two arguments, first --icon and then the value of the Icon key. Should not expand to any arguments if the Icon key is empty or missing.
            %c	The translated name of the application as listed in the appropriate Name key in the desktop entry.
            %k	The location of the desktop file as either a URI (if for example gotten from the vfolder system) or a local filename or empty if no location is known.
            */

            //replace execution variables
            *iter = replace_all(*iter, "%u", search_arguments);
            *iter = replace_all(*iter, "%U", search_arguments);
            *iter = replace_all(*iter, "%f", search_arguments);
            *iter = replace_all(*iter, "%F", search_arguments);
            *iter = replace_all(*iter, "%s", search_arguments);

            spec.args.push_back(*iter);
        }
    }

    return spec;
}

//method for displaying selection details
inline void display_selection_details(){
    Glib::RefPtr<Gtk::TreeSelection> selection = p_autocomplete->get_selection();
    Gtk::TreeModel::Row row = *(selection->get_selected());

    if(row){
        SearchEntry search_entry = row[autocomplete_columns.data];
        p_search_icon->set(row[autocomplete_columns.icon]);
        p_search_arguments_icon->set(row[autocomplete_columns.icon]);
    }
}

//method for retrieving directory list
inline std::vector<Glib::ustring> list_directory(const Glib::ustring& directory){

    //complete, perform lookup under this location if possible
    DIR * p_directory = NULL;
    struct dirent * p_entry = NULL;
    std::vector<Glib::ustring> entries;

    if( (p_directory = opendir(directory.data())) != NULL){

        //populate entries
        while( (p_entry = readdir(p_directory)) != NULL ){

            if(strcmp(p_entry->d_name, ".") == 0 ||
               strcmp(p_entry->d_name, "..") == 0)
                continue;

            entries.push_back(p_entry->d_name);
        }

        closedir(p_directory);
    }

    //sort matches based on name
    std::sort(entries.begin(), entries.end(), sort_directory);

    return entries;
}

//method for auto tab completion of local directory/file lookups
inline Glib::ustring auto_tab(const Glib::ustring& location){
    Glib::ustring complete = location;
    std::vector<Glib::ustring> parts = split(complete, '/');
    Glib::ustring last = parts[parts.size()-1];

    //is last complete yet
    SearchEntryType search_type = get_search_entry_type(replace_all(complete, "~", HOME_DIRECTORY));
    if( (search_type.is_file || search_type.is_directory) &&
       last.compare(".") != 0 &&
       last.compare("..") != 0){

        //get directory listing
        std::vector<Glib::ustring> entries = list_directory(search_type.text);

        //add first entry
        for(auto iter = entries.begin();
            iter < entries.end();
            ++iter){

            parts.push_back( *iter );
            break;
        }

        //reconstruct string
        complete = join(parts, '/');
    }
    else {

        //incomplete, perform lookup from last parent and match
        parts.pop_back();

        Glib::ustring parent_directory = replace_all(join(parts, '/'), "~", HOME_DIRECTORY);

        if(parent_directory.size() == 0)
            parent_directory = "/";

        //get directory listing
        std::vector<Glib::ustring> entries = list_directory(parent_directory.data());

        //iterate directories
        if(!entries.size()){

            //current location is not directory, exit
            return location;
        }

        bool found = false;
        for(auto iter = entries.begin();
            iter < entries.end();
            ++iter){

            Glib::ustring entry_name(*iter);
            if(entry_name.find(last) == 0){

                //found match
                parts.push_back(entry_name);
                found = true;
                break;
            }
        }

        if(!found)
            parts.push_back(last);

        complete = join(parts, '/');
    }

    //add last / if directory
    search_type = get_search_entry_type(replace_all(complete, "~", HOME_DIRECTORY));
    if(complete.at(complete.size()-1) != '/' && search_type.is_directory)
        complete += "/";

    return complete;
}

//method for setting the search icon
inline void set_icon(const Glib::RefPtr<Gdk::Pixbuf>& icon){
    p_search_icon->set(icon);
    p_search_arguments_icon->set(icon);
}







//method for handling search text icon clicks
void on_search_text_icon_press(Gtk::EntryIconPosition icon_position, const GdkEventButton* button){

    if(icon_position == Gtk::ENTRY_ICON_SECONDARY){

    }
}

//method for handling parsing errors
void on_parsing_error(const Glib::RefPtr<const Gtk::CssSection>& section, const Glib::Error& error){
    std::cerr << "CSS Parsing Error: " << error.what() << std::endl;
    if (section){
        std::cerr << "file: " << section->get_file()->get_uri() << std::endl;
        std::cerr << "line: (" << section->get_start_line()+1
            << ", " << section->get_end_line()+1 << ")" << std::endl;
        std::cerr << "position: (" << section->get_start_position()
            << ", " << section->get_end_position() << ")" << std::endl;
    }
}

//method for handling on tree selection change events
void on_tree_selection_changed(){
    display_selection_details();
}

//handle activate events
void on_activate(){

    //get current selection
    Glib::RefPtr<Gtk::TreeSelection> selection = p_autocomplete->get_selection();
    Gtk::TreeModel::Row row = *(selection->get_selected());

    SearchEntry search_entry;
    if(!row){
        //fallback to raw execution of search text
        search_entry.exec = p_search_text->get_text();
    }
    else{
        //retrieve from list selection and launch with exo
        search_entry = row[autocomplete_columns.data];
    }

    //exit if search entry is empty
    if(search_entry.exec.size() == 0){
        p_application->quit();
        return;
    }

    if(verbose)
        std::cout << "Activating: " << search_entry.exec << std::endl;

    ExecSpec spec = parse_exec_path(search_entry.exec);
    Glib::ustring search_text = p_search_text->get_text();

    if(verbose)
        std::cout << "Text: " << search_text << std::endl;

    SearchEntryType search_type = get_search_entry_type(search_text);

    //if not full execution path, find execution path and modify spec.cmd
    if(spec.cmd.at(0) != '/' &&
       spec.cmd.at(0) != '~' &&
       spec.cmd.at(0) != '.'){

        if(verbose)
            std::cout << "Relative execution found." << std::endl;

        std::vector<Glib::ustring> paths = split(getenv("PATH"), ':');

        for(auto iter = paths.begin();
            iter != paths.end();
            ++iter){

            if(file_exists(*iter + "/" + spec.cmd)){
                spec.cmd = *iter + "/" + spec.cmd;
                break;
            }
        }

        if(verbose)
            std::cout << "Absolute execution: " << spec.cmd << std::endl;
    }
    else {

        if(verbose)
            std::cout << "Absolute execution found." << std::endl;

        Glib::ustring location = trim(p_search_text->get_text());

        if(location.at(0) == '~')
            location = replace_all(location, "~", HOME_DIRECTORY);

        if(verbose)
            std::cout << "Using location: " << location << std::endl;

        if(search_type.is_directory){
            spec.args.clear();
            spec.cmd = Glib::ustring("/usr/bin/exo-open");
            spec.args.push_back(Glib::ustring("exo-open"));
            spec.args.push_back(Glib::ustring("--launch"));
            spec.args.push_back(Glib::ustring("FileManager"));
            spec.args.push_back(location);
        }
        else if(search_type.is_file){
            spec.args.clear();
            spec.cmd = Glib::ustring("/usr/bin/exo-open");
            spec.args.push_back(Glib::ustring("exo-open"));
            spec.args.push_back(location);
        }
    }

    //verify executable
    if(!file_exists(spec.cmd) && !search_type.is_application){

        if(verbose)
            std::cout << "Executing web browser command." << std::endl;

        //web browser
        spec.args.clear();
        spec.cmd = Glib::ustring("/usr/bin/exo-open");
        spec.args.push_back(Glib::ustring("exo-open"));
        spec.args.push_back(Glib::ustring("--launch"));

        if(search_type.is_ftp){

            //add url to file manager launch
            spec.args.push_back(Glib::ustring("FileManager"));
            spec.args.push_back(search_type.text);
        }
        else if(search_type.is_url){

            //add url to browser launch
            spec.args.push_back(Glib::ustring("WebBrowser"));
            spec.args.push_back(search_type.text);
        }
        else{

            if(verbose)
                std::cout << "Web browser search." << std::endl;

            spec.args.push_back(Glib::ustring("WebBrowser"));

            //replace %s character in search engine itegration
            char buffer[1024];
            sprintf(buffer, web_search.data(), search_type.text.data());
            spec.args.push_back(Glib::ustring(buffer));
        }
    }

    if(verbose)
        std::cout << "Calling launch process: " << spec.cmd << std::endl;

    //launch external process
    launch_process(spec.cmd, spec.args);

    //terminate current application
    p_application->quit();
}

//method for handling tree row activated events
void on_row_activated(const Gtk::TreePath& tree_path, Gtk::TreeViewColumn * column){

    //forward event to window handler
    on_activate();
}

//handle search application button clicks
void on_search_application_clicked(){
    p_search_arguments->set_text("");
    p_search_icon->show();
    p_search_text->show();
    p_arguments_layout->hide();
    p_search_text->grab_focus_without_selecting();
}

//handle search arcuments key press events
bool on_search_arguments_key_press(const GdkEventKey * key_event){

    if(key_event->keyval == GDK_KEY_Escape){

		p_application->quit();
        return true;
	}

    if(key_event->keyval == GDK_KEY_Tab){

        //perform auto complete on local directory and files
        Glib::ustring argumentsText = p_search_arguments->get_text();
        if(argumentsText.at(0) == '/' ||
           argumentsText.at(0) == '~' ||
           argumentsText.at(0) == '.'){

            //perform autocomplete lookup
            p_search_arguments->set_text(auto_tab(argumentsText));
            p_search_arguments->set_position(p_search_arguments->get_text().size());
        }
        return true;

    }

    //reverse application lock
    if(key_event->keyval == GDK_KEY_BackSpace && p_search_arguments->get_position() == 0){

        on_search_application_clicked();
        return true;
    }

    return false;
}

//handle main window keypress events
bool on_search_text_key_press(const GdkEventKey * key_event){

    //auto add www.<value>.com on control + enter
    if(key_event->state & Gdk::CONTROL_MASK && key_event->keyval == GDK_KEY_Return){
        Glib::ustring search_text = p_search_text->get_text();
        if(search_text.find("www.") == Glib::ustring::npos){
            p_search_text->set_text("www." + search_text);
        }

        search_text = p_search_text->get_text();
        if(search_text.find(".com") == Glib::ustring::npos){
            p_search_text->set_text(search_text + ".com");
        }
    }

    if(key_event->keyval == GDK_KEY_Tab){

        //perform auto complete on local directory and files
        Glib::ustring search_text = p_search_text->get_text();
        if(search_text.at(0) == '/' ||
           search_text.at(0) == '~' ||
           search_text.at(0) == '.'){

            //perform autocomplete lookup
            p_search_text->set_text(auto_tab(search_text));
            p_search_text->set_position(p_search_text->get_text().size());
        }
        else {
            Glib::RefPtr<Gtk::TreeSelection> selection = p_autocomplete->get_selection();
            Gtk::TreeModel::Row row = *(selection->get_selected());

            if(!row)
                return true;

            SearchEntry search_entry = row[autocomplete_columns.data];
            p_search_text->set_text(search_entry.name);
            p_search_text->set_position(search_entry.name.size());

            p_search_icon->hide();
            p_search_text->hide();
            p_arguments_layout->show();

            p_search_application->set_label(search_entry.name);

            p_search_arguments->grab_focus();
        }

        return true;

    }

	if(key_event->keyval == GDK_KEY_Escape){

		p_application->quit();
        return true;
	}

    if(key_event->keyval == GDK_KEY_Up){

        Glib::RefPtr<Gtk::TreeSelection> selection = p_autocomplete->get_selection();
        auto iter = selection->get_selected();
        Gtk::TreeModel::Children children = p_list_store->children();

        if(!iter)
            return false;

        if(iter != children.begin())
            iter--;

        if(!iter)
            return false;

        selection->select(iter);
    }

    if(key_event->keyval == GDK_KEY_Down){

        Glib::RefPtr<Gtk::TreeSelection> selection = p_autocomplete->get_selection();
        auto iter = selection->get_selected();
        Gtk::TreeModel::Children children = p_list_store->children();

        if(!iter)
            return false;

        if(iter != children.end())
            iter++;

        if(!iter)
            return false;

        selection->select(iter);
    }

	return false;
}

//handle SearchText change events
void on_search_text_change(){

	Glib::ustring search_text = p_search_text->get_text();
    std::vector<SearchEntry> matches;
    std::unordered_map<std::string, bool> registry;

    //reset tree view store
    p_list_store->clear();

    //if a character has been entered, begin search
    if(search_text.size()){

        for(auto dir_iter = directories.begin();
            dir_iter < directories.end();
            ++dir_iter){

            Glib::ustring directory_text = *dir_iter;


            DIR * p_directory = NULL;
            struct dirent * p_entry = NULL;

            //iterate directories
            if( (p_directory = opendir(directory_text.data())) == NULL){
                std::cerr << "Error: Could not open directory " << directory_text << std::endl;
                return;
            }

            while( (p_entry = readdir(p_directory)) != NULL ){

                //verify file extension
                Glib::ustring filename = directory_text + "/" + p_entry->d_name;
                std::size_t found = filename.rfind(file_extension);

                if(found == std::string::npos){
                    //incorrect file extension found
                    continue;
                }

                //push onto stack
                matches.push_back(SearchEntry());
                SearchEntry * p_search_entry = &(matches[matches.size()-1]);
                p_search_entry->filename = p_entry->d_name;
                p_search_entry->directory = directory_text;

                //search file contents

                //open desktop file
                Glib::KeyFile file;
                if(!file.load_from_file(filename)){
                    std::cerr << "Error: Could not open application file " << filename << std::endl;

                    //remove from stack
                    matches.pop_back();
                    continue;
                }

                //Glib::ArrayHandle<Glib::ustring> groups = file.get_groups();

                Glib::ustring group_name("Desktop Entry");

                //check name
                try{
                    p_search_entry->name = file.get_locale_string(
                        group_name, Glib::ustring("Name")
                    );
                }
                catch(const Glib::Error& ex){

                    //remove from stack
                    matches.pop_back();
                    continue;
                }

                p_search_entry->name = trim(p_search_entry->name);

                //verify entry doesn't already exist in registry
                if( registry[ p_search_entry->name.data() ] ){

                    //remove from stack
                    matches.pop_back();
                    continue;
                }

                //check description
                try{
                    p_search_entry->description = file.get_locale_string(
                        group_name, Glib::ustring("Comment")
                    );
                }
                catch(const Glib::Error& ex){
                    p_search_entry->description = "";
                }

                //get exec
                try{
                    p_search_entry->exec = file.get_locale_string(
                        group_name, Glib::ustring("Exec")
                    );
                }
                catch(const Glib::Error& ex){

                    //remove from stack
                    matches.pop_back();
                    continue;
                }

                //get icon
                try{
                    p_search_entry->icon = file.get_locale_string(
                        group_name, Glib::ustring("Icon")
                    );
                }
                catch(const Glib::Error& ex){
                    p_search_entry->icon = "applications-other";
                }

                if(default_browser.compare(trim(p_entry->d_name)) == 0 && !default_browser_icon.size()){
                    default_browser_icon = p_search_entry->icon;
                    if(verbose)
                        std::cout << "Default browser set to: " << p_search_entry->icon << ":" << std::endl;
                }

                //search name case insensitive
                p_search_entry->position = p_search_entry->name.lowercase().find( search_text.lowercase() );

                if(p_search_entry->position == Glib::ustring::npos){
                    //match not found

                    //remove from stack
                    matches.pop_back();
                    continue;
                }

                //add entry to registry
                registry[ p_search_entry->name.data() ] = true;

            }

            closedir(p_directory);
        }

        //sort matches based on position
        std::sort(matches.begin(), matches.end(), sort_position_matches);

        //resize to display limit
        if(matches.size() > results_limit)
            matches.resize(results_limit);

        //sort matches based on name
        std::sort(matches.begin(), matches.end(), sort_name_matches);

        //add matched files to tree view model
        for(auto iter = matches.begin(); iter != matches.end(); iter++){
            SearchEntry * p_entry = &(*iter);
            Gtk::TreeModel::Row row = *(p_list_store->append());

            if(p_entry->icon.size()){

                Glib::RefPtr<Gtk::IconTheme> theme = Gtk::IconTheme::get_default();
                Glib::RefPtr<Gdk::Pixbuf> icon;
                int lookup_flags = Gtk::ICON_LOOKUP_USE_BUILTIN | Gtk::ICON_LOOKUP_FORCE_SIZE;

                try{

                    if(p_entry->icon.at(0) == '/'){
                        //file lookup
                        try{
                            icon = Gdk::Pixbuf::create_from_file(p_entry->icon.data());
                        }
                        catch(Glib::FileError& ex){
                            throw Gdk::PixbufError(Gdk::PixbufError::Code::FAILED, ex.what());
                        }
                    }
                    else {
                        //icon lookup
                        try{
                            icon = theme->load_icon(p_entry->icon, lookup_flags);
                        }
                        catch(Glib::Error& ex){
                            throw Gdk::PixbufError(Gdk::PixbufError::Code::FAILED, ex.what());
                        }
                    }

                }
                catch(Gdk::PixbufError& ex){
                    icon = theme->load_icon("applications-other", lookup_flags);
                }

                row[autocomplete_columns.icon] = icon->scale_simple(24, 24, Gdk::INTERP_TILES);
            }

            row[autocomplete_columns.name] = p_entry->name;
            row[autocomplete_columns.description] = p_entry->description;
            row[autocomplete_columns.data] = *p_entry;
        }


        p_autocomplete->set_cursor(Gtk::TreePath("0"));
    }
    else {
        set_icon(get_theme_icon("discard"));
    }

    //show/hide if results found
    if(matches.size()){
        p_autocomplete_overlay->show();
    }
    else{
        p_autocomplete_overlay->hide();
    }

    //show type specific icons
    SearchEntryType search_type = get_search_entry_type(p_search_text->get_text());

    if(search_type.is_ftp){
        set_icon(get_theme_icon("folder-remote"));
    }
    else if(search_type.is_url || search_type.is_search){
        if(default_browser_icon.size())
            set_icon(get_theme_icon(default_browser_icon));
        else
            set_icon(get_theme_icon("web-browser"));
    }
    else if(search_type.is_command){
        set_icon(get_theme_icon("applications-other"));
    }
    else if(search_type.is_file){
        set_icon(get_theme_icon("text-x-generic"));
    }
    else if(search_type.is_directory){
        set_icon(get_theme_icon("folder"));
    }
}

//method for applying a css file
void apply_css(const Glib::ustring css_file){
    Glib::RefPtr<Gtk::CssProvider> p_css_provider = Gtk::CssProvider::create();
    Glib::RefPtr<Gtk::StyleContext> p_style_context = Gtk::StyleContext::create();
    p_css_provider->signal_parsing_error().connect( sigc::ptr_fun(&on_parsing_error) );

    if(p_css_provider->load_from_path(css_file)){
        Glib::RefPtr<Gdk::Screen> screen = p_main_window->get_screen();
        p_style_context->add_provider_for_screen(screen, p_css_provider, GTK_STYLE_PROVIDER_PRIORITY_USER);
    }
}

//method for displaying command line help
void display_help(){
    std::cout << std::endl;
    std::cout << PACKAGE_STRING << std::endl;
    std::cout << std::endl;
    std::cout << "Usage:" << std::endl;
    std::cout << "  " << PACKAGE_NAME << " [OPTION...]" << std::endl;
    std::cout << std::endl;
    std::cout << "Help Options:" << std::endl;
    std::cout << "  -h, --help\t\tShow help options" << std::endl;
    std::cout << "  -v, --verbose\t\tShow verbose text" << std::endl;
    std::cout << std::endl;
}

int main(int argc, char ** argv){

    trim("hello how are you? ");
    //get default web browser
    //use: xdg-settings get default-web-browser
    default_browser = passthru("xdg-settings get default-web-browser");

    //iterate command line args
    for(int i=0; i<argc; i++){
        if(strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0){
            display_help();
            return 0;
        }

        if(strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0){
            verbose = true;
        }
    }

    //xfce_textdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");

    garcon_set_environment_xdg(GARCON_ENVIRONMENT_XFCE);

    GError * p_error = NULL;
    if(!xfconf_init(&p_error)){
        std::cerr << "Error initializing xfconf:" << p_error->message << std::endl;
        return EXIT_FAILURE;
    }

    //create application instance
	p_application = Gtk::Application::create("com.collaboradev.xfce4-finder");

    //create builder reference for glade
	p_builder = Gtk::Builder::create();

    //load glade
	try{
        p_builder->add_from_string(GLADE_XML);
	}
	catch(const Glib::FileError& ex){
		std::cerr << "Glade FileError: " << ex.what() << std::endl;
		return EXIT_FAILURE;
	}
	catch(const Glib::MarkupError& ex){
    	std::cerr << "Glade Markup_error: " << ex.what() << std::endl;
    	return EXIT_FAILURE;
  	}
  	catch(const Gtk::BuilderError& ex){
    	std::cerr << "Glade BuilderError: " << ex.what() << std::endl;
    	return EXIT_FAILURE;
  	}

	//retrieve pointer to main window
	p_builder->get_widget("MainWindow", p_main_window);

	//retrive pointer to search text
	p_builder->get_widget("SearchText", p_search_text);
	p_search_text->signal_changed().connect( sigc::ptr_fun(&on_search_text_change) );
    p_search_text->signal_key_press_event().connect( sigc::ptr_fun(&on_search_text_key_press), false );
    p_search_text->signal_activate().connect( sigc::ptr_fun(&on_activate) );
    p_search_text->signal_icon_press().connect( sigc::ptr_fun(&on_search_text_icon_press) );

    //retrieve pointer to search icon
    p_builder->get_widget("SearchIcon", p_search_icon);
    p_builder->get_widget("SearchArgumentsIcon", p_search_arguments_icon);

    //settings icon lookup
    try{
        //Glib::RefPtr<Gdk::Pixbuf> icon = get_theme_icon("applications-system");
        //p_search_text->set_icon_from_pixbuf(icon, Gtk::ENTRY_ICON_SECONDARY);
    }
    catch(Glib::Error& ex){}

    p_builder->get_widget("ArgumentsLayout", p_arguments_layout);

    p_builder->get_widget("SearchApplication", p_search_application);
    p_search_application->signal_clicked().connect( sigc::ptr_fun(&on_search_application_clicked) );

    //retrieve pointer to search arguments
    p_builder->get_widget("SearchArguments", p_search_arguments);
    p_search_arguments->signal_key_press_event().connect( sigc::ptr_fun(&on_search_arguments_key_press), false );
    p_search_arguments->signal_activate().connect( sigc::ptr_fun(&on_activate) );

    //retrive pointer to autocomplete list view
    p_builder->get_widget("Autocomplete", p_autocomplete);

    //retrive pointer to autocomplete overlay
    p_builder->get_widget("AutocompleteOverlay", p_autocomplete_overlay);
    p_autocomplete_overlay->hide();

    //create list view store
    p_list_store = Gtk::ListStore::create(autocomplete_columns);
    p_autocomplete->set_model(p_list_store);
    p_autocomplete->append_column("Icon", autocomplete_columns.icon);
    p_autocomplete->append_column("Name", autocomplete_columns.name);
    p_autocomplete->append_column("Description", autocomplete_columns.description);


    p_autocomplete->signal_row_activated().connect( sigc::ptr_fun(&on_row_activated) );
    Glib::RefPtr<Gtk::TreeSelection> p_tree_selection = p_autocomplete->get_selection();
    p_tree_selection->signal_changed().connect( sigc::ptr_fun(&on_tree_selection_changed) );

    //add css support
    Glib::ustring finder_directory = HOME_DIRECTORY + "/.config/xfce4/finder/";
    Glib::ustring css_file = finder_directory + theme + ".css";

    if(file_exists(css_file)){
        apply_css(css_file);
    }
    else {
        //install default CSS
        if(!file_exists(finder_directory))
            mkdir(finder_directory.data(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

        write_file(css_file, pretty_print_css(DEFAULT_CSS));
        apply_css(css_file);
    }

    //use custom css
    CustomCss css;
    results_limit = css.results_property.get_value();
    p_main_window->resize(css.width_property.get_value(), css.height_property.get_value());
    p_main_window->set_position(Gtk::WIN_POS_CENTER_ALWAYS);

    //load default configuration settings
    load_channel_settings();

    //run main application
	p_application->run(*p_main_window);

    xfconf_shutdown();

	return EXIT_SUCCESS;
}


