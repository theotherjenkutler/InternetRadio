//
// parsingTools.h

//
// Some simple inline utils .. cause it's faster than using Arduino Strings

inline boolean isDigit(const char c, int *dots)
{
    if ('.'==c) { (*dots)++; return true; }
    return ((c>='0') && (c<='9'));
}

inline int isNumeric(const char *s)
{
    int dots = 0;
    while (*s) {
        if (!isDigit(*s, &dots)) {
            return 0;
        }
        s++;
    }
    return 1+dots;
}

inline const char *checkForEnd(const char *text, const char *end)
{
    if (text>=end) { return NULL; }
    return text;
}

inline const char *clearNewlines(const char *text)
{
    while (true) {
        switch (*text) {
            case '\n' :
            case '\r' :
                text++;
                break;
            default :
                return text;
                break;
        }
    }
}

inline const char *clearWhitespace(const char *text)
{
    while(true) {
        switch (*text) {
            case '\t' :
            case ' ' :
            case '=' :
                text++;
                break;
            default :
                return text;
                break;
        }
    }
}

// returns pointer to 1 character after the string
inline const char *findEndOfString(const char *text)
{
    while(true) {
        switch (*text) {
            case '\0' :
            case '\t' :
            case '\n' :
            case ' ' :
            case '=' :
                return text;
                break;
            default :
                text++;
                break;
        }
    }
}

inline boolean isTerminator(const char *text)
{
    switch (*text) {
        case '\0' :
        case '\n' :
        case '\r' : // maybe someone uses and old text editor
            return true;
            break;
    }
    return false;
}

inline const char *skipToTerminatar(const char *text)
{
    while (!isTerminator(text)) { text++; }
    return text;
}

//
// END OF parsingTools.h
