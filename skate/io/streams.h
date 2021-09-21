#ifndef SKATE_HEXBUF_H
#define SKATE_HEXBUF_H

#include <streambuf>

namespace skate {
    // Simple buffer that encodes all content as hex output, with optional uppercase/lowercase and trailing spaces after every nibble
    template<typename Char, typename Traits = std::char_traits<Char>>
    class basic_hexencodebuf : public std::basic_streambuf<Char, Traits> {
        std::basic_streambuf<Char, Traits> *base;
        const char *alphabet;
        bool spaces;

    public:
        using typename std::basic_streambuf<Char, Traits>::traits_type;
        using typename std::basic_streambuf<Char, Traits>::int_type;
        using typename std::basic_streambuf<Char, Traits>::char_type;

        basic_hexencodebuf(std::basic_streambuf<Char, traits_type> *base, bool uppercase = false, bool spaces = false)
            : base(base)
            , alphabet(uppercase? "0123456789ABCDEF": "0123456789abcdef")
            , spaces(spaces)
        {}

    protected:
        virtual int_type overflow(int_type c) override {
            if (traits_type::eq_int_type(c, traits_type::eof()))
                return traits_type::not_eof(c);
            else if (!base)
                return traits_type::eof();

            typedef typename std::make_unsigned<Char>::type UChar;
            const UChar u = c;

            for (size_t i = std::numeric_limits<UChar>::digits / 8; i; --i) {
                const unsigned char b = (u >> ((i - 1) * 8)) & 0xff;

                if (traits_type::eq_int_type(base->sputc(traits_type::to_char_type(alphabet[b >> 4])), traits_type::eof()) ||
                    traits_type::eq_int_type(base->sputc(traits_type::to_char_type(alphabet[b & 0xf])), traits_type::eof()) ||
                    (spaces && traits_type::eq_int_type(base->sputc(' '), traits_type::eof())))
                    return traits_type::eof();
            }

            return c;
        }
    };

    typedef basic_hexencodebuf<char> hexencodebuf;

    // Simple buffer that copies all content to two buffers
    template<typename Char, typename Traits = std::char_traits<Char>>
    class basic_teebuf : public std::basic_streambuf<Char, Traits> {
        std::basic_streambuf<Char, Traits> *lhs, *rhs;

    public:
        using typename std::basic_streambuf<Char, Traits>::traits_type;
        using typename std::basic_streambuf<Char, Traits>::int_type;
        using typename std::basic_streambuf<Char, Traits>::char_type;

        basic_teebuf(std::basic_streambuf<Char, traits_type> *lhs, std::basic_streambuf<Char, traits_type> *rhs)
            : lhs(lhs)
            , rhs(rhs)
        {}

    protected:
        virtual std::streamsize xsputn(const char_type *s, std::streamsize n) override {
            if (lhs && rhs)
                return std::min(lhs->sputn(s, n), rhs->sputn(s, n));
            else if (lhs)
                return lhs->sputn(s, n);
            else if (rhs)
                return rhs->sputn(s, n);
            else
                return 0;
        }

        virtual int_type overflow(int_type c) override {
            if (traits_type::eq_int_type(c, traits_type::eof()))
                return traits_type::not_eof(c);
            else if (!lhs && !rhs)
                return traits_type::eof();

            if ((lhs && traits_type::eq_int_type(lhs->sputc(traits_type::to_char_type(c)), traits_type::eof())) ||
                (rhs && traits_type::eq_int_type(rhs->sputc(traits_type::to_char_type(c)), traits_type::eof())))
                return traits_type::eof();

            return c;
        }
    };

    typedef basic_teebuf<char> teebuf;
    typedef basic_teebuf<wchar_t> wteebuf;

    // Simple buffer that reads/writes to a C FILE * object
    template<typename Char, typename Traits = std::char_traits<Char>>
    class basic_cfilebuf : public std::basic_streambuf<Char, Traits> {
        FILE *file;
        bool owned;
        Char in;

    public:
        using typename std::basic_streambuf<Char, Traits>::traits_type;
        using typename std::basic_streambuf<Char, Traits>::int_type;
        using typename std::basic_streambuf<Char, Traits>::char_type;

        basic_cfilebuf(FILE *file = nullptr)
            : file(file)
            , owned(false)
            , in(0)
        {}
        ~basic_cfilebuf() {
            if (owned)
                close();
        }

        basic_cfilebuf *open(const char *filename, std::ios_base::openmode mode) {
            if (file)
                return nullptr;

            char flags[8] = { 0 };
            char *flags_ptr = flags;

            if (mode & std::ios_base::binary)
                *flags_ptr++ = 'b';

            if (mode & std::ios_base::in)
                *flags_ptr++ = 'r';

            if (mode & std::ios_base::out) {
                if (mode & std::ios_base::app)
                    *flags_ptr++ = 'a';
                else
                    *flags_ptr++ = 'w';

                if ((mode & std::ios_base::trunc) == 0)
                    *flags_ptr++ = '+';
            }

            file = fopen(filename, flags);
            if (!file)
                return nullptr;

            if (mode & std::ios_base::ate) {
                if (fseek(file, 0, SEEK_END)) {
                    fclose(file);
                    return nullptr;
                }
            }

            owned = true;

            return this;
        }
        bool is_open() const { return file; }
        basic_cfilebuf *close() {
            if (!file)
                return nullptr;

            fclose(file);

            file = nullptr;
            owned = false;

            return this;
        }

    protected:
        // TODO: handle seeking and proper character putback

        virtual std::streamsize xsgetn(char_type *s, std::streamsize n) override {
            if (!file)
                return 0;

            return fread(s, sizeof(char_type), size_t(n), file);
        }

        virtual int_type underflow() override {
            if (!file)
                return traits_type::eof();

            if (fread(&in, sizeof(char_type), 1, file) != 1)
                return traits_type::eof();

            std::basic_streambuf<Char, traits_type>::setg(&in, &in, &in + 1);

            return traits_type::to_int_type(in);
        }

        virtual int_type pbackfail(int_type c) override {
            if (!file)
                return traits_type::eof();
            else {
                // TODO: handle new replacement character putback
                return fseek(file, -int(sizeof(char_type)), SEEK_CUR)? traits_type::eof(): traits_type::not_eof(c);
            }
        }

        virtual std::basic_streambuf<Char, traits_type> *setbuf(char_type *s, std::streamsize n) override {
            // No good way to specify if an error occurred when calling setvbuf
            setvbuf(file, s, _IOFBF, size_t(n));

            return this;
        }

        virtual int sync() override {
            return !file || fflush(file)? -1: 0;
        }

        virtual std::streamsize xsputn(const char_type *s, std::streamsize n) override {
            if (!file)
                return 0;

            return fwrite(s, sizeof(char_type), size_t(n), file);
        }

        virtual int_type overflow(int_type c) override {
            if (traits_type::eq_int_type(c, traits_type::eof()))
                return traits_type::not_eof(c);
            else if (!file)
                return traits_type::eof();

            const char_type chr = traits_type::to_char_type(c);

            return fwrite(&chr, sizeof(char_type), 1, file) == 1? c: traits_type::eof();
        }
    };

    typedef basic_cfilebuf<char> cfilebuf;
    typedef basic_cfilebuf<wchar_t> wcfilebuf;
}

#endif // SKATE_HEXBUF_H
