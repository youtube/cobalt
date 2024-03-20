#ifndef FAKE_CED
#define FAKE_CED

enum Encoding {
   UNKNOWN_ENCODING
};

//constexpr int UNKNOWN_ENCODING = 1;
constexpr int UNKNOWN_LANGUAGE = 2;

struct CompactEncDet {
    enum _foo_unused {
        QUERY_CORPUS
    };

    static Encoding DetectEncoding(
        const char *, size_t , std::nullptr_t, std::nullptr_t , std::nullptr_t,
        Encoding, const int &, _foo_unused, bool, int *, bool *);
};

std::string MimeEncodingName(Encoding);

#endif
