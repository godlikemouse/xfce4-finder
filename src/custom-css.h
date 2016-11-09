#ifndef __CUSTOM_CSS_H
#define __CUSTOM_CSS_H

class CustomCss : public Gtk::Widget {
    public:

    Gtk::StyleProperty<int> width_property;
    int width;

    Gtk::StyleProperty<int> height_property;
    int height;

    Gtk::StyleProperty<int> results_property;
    int results;

    //constructor
    CustomCss() :
        Glib::ObjectBase("finder"),
        Gtk::Widget(),
        //-gtkmm__CustomObject_customcss-width
        width_property(*this, "width", 1000),
        width(1000),

        //-gtkmm__CustomObject_customcss-height
        height_property(*this, "height", 400),
        height(400),

        //-gtkmm__CustomObject_customcss-results
        results_property(*this, "results", 10),
        results(10)
        {}
};

#endif
