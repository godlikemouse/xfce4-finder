
# xfce4-finder
Smart and intuitive application finder, complete with theme and customization support.

![Screenshot](https://cloud.githubusercontent.com/assets/7003154/20498215/3f256b5c-affa-11e6-9f6f-9fcdc8b94f08.png)

## Key Features
- Blazingly fast realtime application finding
- Inline application argument launching using tab
    - Begin by typing an application name, hit TAB_KEY to lock it in, begin typing arguments to that application *(ie. "mail" TAB_KEY "someone@someplace.com" ENTER_KEY)*.
- Customizable UI and theme using CSS
- Intuitive Web/Desktop Integration
    - Just type what you're looking for, the application will determine whether or not an application should be launched or if the context requires an online web search.
- Direct command invocation
    - Utilize terminal commands directly by typing the executable name *(ie. "gedit" ENTER_KEY)*.
- Supports tab autocomplete for local application and filenames
- Supports CONTROL_KEY + ENTER_KEY for domain name completion *(ie. google => www.google.com, google.com => www.google.com, www.google => www.google.com)*.

## Customizable Settings <a name="customizable-settings"></a>
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

Set array values:

	xfconf-query -c xfce4-finder -p /application-directories -s /usr/share/applications -s /usr/local/share/applications

The above sets the application-directories array property which is used by xfce4-finder to location application files to /usr/share/applications and /usr/local/share/applications.  You may need to update this setting if your applications directory is not included by default.

## Installation
This application is available under the Arch Linux AUR repository as a [PKGBUILD](https://aur.archlinux.org/packages/xfce4-finder/).  To install using yaourt, use the following command:

	yaourt xfce4-finder

## Building
To build the application you will need to have the following dependencies installed.
- [Glib 2.0](https://developer.gnome.org/glib/)
- [Gio 2.0](https://developer.gnome.org/gio/)
- [Gtkmm 3.0](http://www.gtkmm.org/en/)
- [Garcon](http://www.linuxfromscratch.org/blfs/view/svn/xfce/garcon.html)
- [libxfconf](http://www.linuxfromscratch.org/blfs/view/systemd/xfce/xfconf.html)
- [libxfce4util](http://www.linuxfromscratch.org/blfs/view/7.9/xfce/libxfce4util.html)
- [libxfce4ui](http://www.linuxfromscratch.org/blfs/view/systemd/xfce/libxfce4ui.html)
- [automake](https://www.gnu.org/software/automake/)
- [autoconf](https://www.gnu.org/software/autoconf/autoconf.html)
- [xfce4-dev-tools](http://www.xfce.org/)

On Arch Linux:

    sudo pacman -S glib2 gtkmm garcon xfconf libxfce4util libxfce4ui automake autoconf xfce4-dev-tools



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

## Troubleshooting

- The application won't run resulting the error: Error: Could not open directory /home/USER/.local/share/applications

    * Update the application-directories setting using the Set array values example listed under [Customizable Settings](#customizable-settings).


## Community

Keep track of development and community news.

* Follow [@Collaboradev on Twitter](https://twitter.com/collaboradev).
* Follow the [Collaboradev Blog](http://www.collaboradev.com).

## License

xfce4-finder is released under the MIT License.
