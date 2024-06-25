/*
 * This is an integer ascii Mandelbrot generator
 */
var left_edge = -420;
var right_edge = 300;
var top_edge = 300;
var bottom_edge = -300;
var x_step = 7;
var y_step = 15;
/* how many iterations to do. higher values increase precision, but take longer, obviously. */
var max_iter = 500;
var y0 = top_edge;
while (y0 > bottom_edge)
{
    var x0 = left_edge;
    while (x0 < right_edge)
    {
        var y = 0;
        var x = 0;
        /* 32 = ' ' */
        var the_char = 32;
        var i = 0;
        while (i < max_iter)
        {
            var x_x = (x * x) / 200;
            var y_y = (y * y) / 200;
            if (x_x + y_y > 800)
            {
                /* 48 = '0' */
                the_char = 48 + i;
                if (i > 9)
                {
                    /* 64 = '@' */
                    the_char = 64;
                }
                i = max_iter;
            }
            y = x * y / 100 + y0;
            x = x_x - y_y + x0;
            i = i + 1;
        }
        print(String.fromCharCode(the_char));
        x0 = x0 + x_step;
    }
    y0 = y0 - y_step;
    print("\n");
}
