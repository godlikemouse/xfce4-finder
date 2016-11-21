# xfce4-finder
Smart and intuitive application finder, complete with theme and customization support.

![Screenshot](https://cloud.githubusercontent.com/assets/7003154/20498215/3f256b5c-affa-11e6-9f6f-9fcdc8b94f08.png)

## Key Features
- Blazingly fast realtime application finding
- Inline application argument launching using tab
    - Beging typing an application name, hit TAB_KEY to lock it in, begin typing arguments to that application *(ie. Mail TAB_KEY someone@someplace.com ENTER_KEY)*.
- Customizable UI and theme using CSS
- Intuitive Web/Desktop Integration
    - Just begin typing, the application will determine whether or not an application should be launched or if the context requires an online web search.
- Direct command invocation
    - Utilize terminal command directly by typing the executable name *(ie. gedit ENTER_KEY)*.
- Supports tab autocomplete for local application and filenames
- Supports CONTROL_KEY + ENTER_KEY for domain name completion *(ie. google => www.google.com, google.com => www.google.com, www.google => www.google.com)*.

## Customizable Settings
Application settings can be controlled using xfconf-query.

Available settings:
- /file-extension - defaults to ".desktop", controls the filenames to use for parsing.
- /web-search - defaults to "https://www.google.com/#safe=on&q=%s", controls the url format to use when applying a web search, %s is replaced by the query to be used.
- /theme = defaults to "default", controls the CSS file to use for the application *(Default location is: ~/.config/xfce4/finder/default.css)*.
- /application-directories - defaults to /usr/share/applications and ~/.local/share/applications, specifies the directories in which to look for application files with matching /file-extension.

List all available settings:

    xfconf-query -c xfce4-finder -l

List a setting:

    xfconf-query -c xfce4-finder -p /web-search

Change a setting:

    xfconf-query -c xfce4-finder -p /web-search -s https://www.bing.com?q=%s

## Building
To build the application you will need to have Xfce development tools installed ([xfce4-dev-tools](http://www.xfce.org/)).

On Arch Linux:

    sudo pacman -S xfce4-dev-tools

Building the application:

    cd xfce4-finder/
    ./autogen.sh
    make
    make install

## Running
Open xfce4-keyboard-settings and bind a key combination to use for the xfce4-finder such as META_KEY + SPACE_KEY or ALT_KEY + SPACE_KEY and invoke /usr/bin/xfce4-finder.

Once the application is displayed, begin typing.  Execute commands with either the ENTER_KEY or by mouse clicking a selection in the autocomplete list.

## Command Arguments

    xfce4-finder 1.0

    Usage:
      xfce4-finder [OPTION...]

    Help Options:
      -h, --help	        Show help options
      -v, --verbose         Show verbose text

## Features/Issues
Feel free add feature requests or report issues: [Features/Issues](https://github.com/godlikemouse/xfce4-finder/issues).

## Community

Keep track of development and community news.

* Follow [@Collaboradev on Twitter](https://twitter.com/collaboradev).
* Follow the [Collaboradev Blog](http://www.collaboradev.com).

## License

xfce4-finder is released under the MIT License.
