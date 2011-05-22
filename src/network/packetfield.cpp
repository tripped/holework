// packetfield.cpp
//
#include "packetfield.h"

#include "uniconv.h"
#include <gtest/gtest.h>

using namespace boostcraft::network;

namespace boostcraft { namespace network {

/*
 * Converter functions for StringFields
 */
std::string NullConverter(std::string const& str)
{
    return str;
}

std::string UCS2Converter(std::u16string const& str)
{
    return ucs2toutf8(str);
}

}} // namespace boostcraft::network

TEST(PacketFieldTests, ByteField)
{
    boost::asio::streambuf b;
    std::ostream os(&b);
    os.put(0x42);

    uint8_t val = 0;
    ByteField field(val);
    field.readFrom(b);

    ASSERT_EQ(0x42, val);
}

TEST(PacketFieldTests, ShortField)
{
    boost::asio::streambuf b;
    std::ostream os(&b);
    os.put(0x10);
    os.put(0x80);

    uint16_t val = 0;
    ShortField field(val);
    field.readFrom(b);

    ASSERT_EQ(0x1080, val);
}

TEST(PacketFieldTests, LongField)
{
    boost::asio::streambuf b;
    std::ostream os(&b);
    os.put(0x10);
    os.put(0x20);
    os.put(0x30);
    os.put(0x40);
    os.put(0x50);
    os.put(0x60);
    os.put(0x70);
    os.put(0x80);

    uint64_t val = 0;
    LongField field(val);
    field.readFrom(b);

    ASSERT_EQ(0x1020304050607080LLU, val);
}

TEST(PacketFieldTests, DoubleField)
{
    boost::asio::streambuf b;
    std::ostream os(&b);
    
    // Double value 1337.1337 in network byte order:
    os.put(0x40);
    os.put(0x94);
    os.put(0xE4);
    os.put(0x88);
    os.put(0xE8);
    os.put(0xA7);
    os.put(0x1D);
    os.put(0xE7);

    double val = 0.0d;

    DoubleField field(val);
    field.readFrom(b);

    // This number should be exactly representable
    ASSERT_EQ(1337.1337, val);
}

TEST(PacketFieldTests, DoubleFieldContinuable)
{
    boost::asio::streambuf b;
    std::ostream os(&b);

    double val = 0.0d;

    DoubleField field(val);

    // All 8 bytes should still be required at first
    ASSERT_EQ(8, field.readFrom(b));

    // Double value 4.6855785559756050e-277
    // Put only the first three bytes in the buffer
    os.put(0x06);
    os.put(0x90);
    os.put(0x9c);

    // Should need five more bytes
    ASSERT_EQ(5, field.readFrom(b));

    os.put(0xa5);

    ASSERT_EQ(4, field.readFrom(b));

    os.put(0x45);
    os.put(0x8c);
    os.put(0xa9);
    os.put(0xc6);

    ASSERT_EQ(0, field.readFrom(b));

    // This value should be exactly represented so direct equality is OK
    ASSERT_EQ(4.6855785559756050e-277, val);
}

/*
 * Verify that UTF-8 strings are read correctly across multiple buffers.
 */
TEST(PacketFieldTests, UtfStringContinuable)
{
    boost::asio::streambuf b;
    std::ostream os(&b);

    /*
     * For this test, one quatrain of the Rubaiyat of Omar Khayyam, in Farsi
     * and Fitzgerald's English, plus one quatrain of the Rubaiyat of a Persian
     * kitten, in plain and simple everyday unoriental speech.
     */
    std::string teststr(
        u8"Why, all the Saints and Sages who discuss'd\n"
        "Of the Two Worlds so wisely -- they are thrust\n"
        "Like foolish Prophets forth; their Words to Scorn\n"
        "Are scatter'd, and their Mouths are stopt with Dust.\n"
        "\n"
        "آنانكه ز پيش رفته‌اند اى ساقى\n"
        "درخاك غرور خفته‌اند اى ساقى\n"
        "رو باده خور و حقيقت از من بشنو\n"
        "باد است هرآنچه گفته‌اند اى ساقى\n"
        "\n"
        "They say the Lion and the Lizard keep\n"
        "The Courts where Jamshyd gloried and drank deep.\n"
        "The Lion is my cousin; I don't know\n"
        "Who Jamshyd is--nor shall it break my sleep.\n");

    os.put(teststr.length() >> 8);
    os.put(teststr.length() & 0xFF);

    std::string val;
    String8Field field(val, 0);

    // Only write the first 42 bytes at first; the first attempt to read should
    // then come up len-42 bytes short and consume all 42 available bytes.
    os << teststr.substr(0, 42);
    size_t bytesleft = field.readFrom(b);
    ASSERT_EQ(teststr.length() - 42, bytesleft);
    ASSERT_EQ(0, b.size());

    // Write the next 112 bytes and try reading from the buffer again
    os << teststr.substr(42, 112);
    bytesleft = field.readFrom(b);
    ASSERT_EQ(teststr.length() - 42 - 112, bytesleft);
    ASSERT_EQ(0, b.size());

    // Finally, write the rest of the string
    os << teststr.substr(42+112);
    bytesleft = field.readFrom(b);
    ASSERT_EQ(0, bytesleft);
    ASSERT_EQ(0, b.size());

    // Check that the read string is the same as the original
    ASSERT_EQ(teststr, val);
}

/*
 * Verify that multiple fields can be read successively from a single buffer
 */
TEST(PacketFieldTests, MultipleFields)
{
    boost::asio::streambuf b;
    std::ostream os(&b);

    // Fields:
    //  (string8) "Avogadro's number (not Avocado) is:"
    //  (float)   0x66FF0C30 = 6.0221414e+23
    //  (byte)    0x2A
    //  (byte)    0xFE
    //  (long)    0xDEADBEEF00C0FFEE
    std::string avocado("Avogadro's number (not Avocado) is:");
    os.put(avocado.length() >> 8);
    os.put(avocado.length() & 0xFF);
    os << avocado;
    std::vector<uint8_t> bytes = { 0x66, 0xFF, 0x0C, 0x30,
        0x2A, 0xFE, 0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0xC0, 0xFF, 0xEE };
    os.write((char*)&bytes[0], bytes.size());

    size_t bufsize = b.size();

    std::string msg;
    float avogadro = 0.0f;
    uint8_t foo = 0;
    uint8_t bar = 0;
    uint64_t baz = 0;

    String8Field msgField(msg, 0);
    FloatField avoField(avogadro);
    ByteField fooField(foo);
    ByteField barField(bar);
    LongField bazField(baz);

    ASSERT_EQ(0, msgField.readFrom(b));
    ASSERT_EQ(avocado, msg);
    ASSERT_EQ(bufsize - 2 - avocado.length(), b.size());
    bufsize -= 2 + avocado.length();

    ASSERT_EQ(0, avoField.readFrom(b));
    ASSERT_FLOAT_EQ(6.0221414e+23, avogadro);
    ASSERT_EQ(bufsize - 4, b.size());
    bufsize -= 4;

    ASSERT_EQ(0, fooField.readFrom(b));
    ASSERT_EQ(0x2A, foo);
    ASSERT_EQ(bufsize - 1, b.size());
    bufsize -= 1;

    ASSERT_EQ(0, barField.readFrom(b));
    ASSERT_EQ(0xFE, bar);
    ASSERT_EQ(bufsize - 1, b.size());
    bufsize -= 1;

    ASSERT_EQ(0, bazField.readFrom(b));
    ASSERT_EQ(0xDEADBEEF00C0FFEE, baz);
    ASSERT_EQ(0, b.size());
}



