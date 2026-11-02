#ifndef NOCTURNE_TYPOGRAPHY_H
#define NOCTURNE_TYPOGRAPHY_H

/* Primary roles deliberately share the Settings app's comfortable body size.
 * Keep separate handles for compatibility with existing callers. */
static inline int Typography_points(int size, int role)
{
    static const int points[4][5] = {
        {9, 9, 9, 8, 8},
        {10, 10, 10, 8, 8},
        {12, 12, 12, 10, 8},
        {14, 14, 14, 12, 9},
    };
    if (size < 0 || size > 3) size = 1;
    if (role < 0 || role > 4) role = 2;
    return points[size][role];
}

static inline int Typography_rowHeight(int text_height, int minimum, int padding)
{
    return text_height + padding > minimum ? text_height + padding : minimum;
}

#endif
