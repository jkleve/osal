#include "osal/os.h"
#include <gtest/gtest.h>
#include <gmock/gmock.h>

class TestOsal : public ::testing::Test {
public:
    TestOsal() {}

    ~TestOsal() override {}

    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(TestOsal, sleep)
{
    auto t1 = os::time_since_epoch();
    os::sleep(1000);
    auto t2 = os::time_since_epoch();
    EXPECT_GT(t2, t1);
    EXPECT_LE(t2, t1 + 2);
}

TEST_F(TestOsal, dump_and_read_nominal)
{
    std::string data("Hope you have a good day");
    std::string file("./test1.txt");
    os::file::dump(file.data(), data.data(), data.size());

    EXPECT_TRUE(os::file::size(file.data()) > 0);
    EXPECT_TRUE(os::file::is_reg_file(file.data()));

    auto read_data = os::file::read(file);

    EXPECT_EQ(read_data.num_bytes, data.size());
    EXPECT_STREQ(data.c_str(), read_data.data.get());
}

TEST_F(TestOsal, dump_and_read_international)
{
    std::string data(u8"Hope you have a 坏 day");
    std::string file(u8"./亁井.txt");
    os::file::dump(file.data(), data.data(), data.size());

    EXPECT_TRUE(os::file::size(file.data()) > 0);
    EXPECT_TRUE(os::file::is_reg_file(file.data()));

    auto read_data = os::file::read(file);

    EXPECT_EQ(read_data.num_bytes, data.size());
    EXPECT_STREQ(data.c_str(), read_data.data.get());
}

TEST_F(TestOsal, open_close)
{
    std::string data("Hope you have a good day");
    std::string file("./test1.txt");
    os::file::dump(file.data(), data.data(), data.size() + 1);

    auto fd = os::file::open(file.c_str(), "rb");
    ASSERT_NE(fd, nullptr);

    auto buffer = std::unique_ptr<char>(new char[data.size()+1]);
    auto read = fread(buffer.get(), 1, data.size() + 1, fd);
    EXPECT_EQ(os::file::close(fd), 0);

    EXPECT_EQ(data.size() + 1, read);
    EXPECT_EQ(memcmp(data.data(), buffer.get(), data.size()), 0);
    EXPECT_TRUE(os::file::delete_file(file));
}

TEST_F(TestOsal, copy_file)
{
    std::string data("Hope you have a good day");
    std::string src("./src.txt");
    std::string dst("./dst.txt");
    os::file::dump(src.data(), data.data(), data.size());

    EXPECT_TRUE(os::file::size(src.data()) > 0);
    EXPECT_TRUE(os::file::is_reg_file(src.data()));
    EXPECT_FALSE(os::file::is_reg_file(dst.data()));
    EXPECT_TRUE(os::file::copy_file(src, dst));
    EXPECT_TRUE(os::file::is_reg_file(src.data()));
    EXPECT_TRUE(os::file::is_reg_file(dst.data()));

    auto dst_data = os::file::read(dst);
    EXPECT_TRUE(memcmp(data.data(), dst_data.data.get(), data.size()) == 0);

    EXPECT_TRUE(os::file::delete_file(src) && os::file::delete_file(dst));
}

TEST_F(TestOsal, touch)
{
    const char* file = "touch.txt";
    EXPECT_TRUE(os::file::touch(file));
    EXPECT_TRUE(os::file::is_reg_file(file));
    EXPECT_EQ(os::file::size(file), 0);
    EXPECT_TRUE(os::file::delete_file(file));
    EXPECT_FALSE(os::file::is_reg_file(file));
}

TEST_F(TestOsal, create_dir)
{
    const char* dir1 = u8"./test_dir";
    const char* dir2 = u8"./test_dir/sub";
    const char* file1 = u8"./test_dir/file1.txt";
    const char* file2 = u8"./test_dir/file2.txt";
    const char* file3 = u8"./test_dir/sub/file3.txt";
    EXPECT_TRUE(os::file::delete_dir(dir1)); // possible remnants of previous runs

    EXPECT_TRUE(os::file::create_dir(dir1));
    EXPECT_TRUE(os::file::create_dir(dir2));
    EXPECT_TRUE(os::file::touch(file1));
    EXPECT_TRUE(os::file::touch(file2));
    EXPECT_TRUE(os::file::touch(file3));

    auto files = os::file::list_dir(dir1);
    EXPECT_EQ(files.size(), 2);
    EXPECT_TRUE(memcmp(files.front().data(), "file1.txt", 9) == 0 || memcmp(files.front().data(), "file2.txt", 9) == 0);
    EXPECT_TRUE(memcmp(files.back().data(), "file1.txt", 9) == 0 || memcmp(files.back().data(), "file2.txt", 9) == 0);
    EXPECT_STRNE(files.front().data(), files.back().data());

    files = os::file::list_dir(dir2);
    EXPECT_EQ(files.size(), 1);
    EXPECT_TRUE(memcmp(files.front().data(), "file3.txt", 9) == 0);
    EXPECT_TRUE(memcmp(files.back().data(), "file3.txt", 9) == 0);
    EXPECT_STREQ(files.front().data(), files.back().data());

    // cleanup
    EXPECT_TRUE(os::file::delete_dir(dir1));
    EXPECT_FALSE(os::file::is_dir(dir1));
}

TEST_F(TestOsal, create_dir_international_chars)
{
    auto dir1 = u8"./Äér";
    auto dir2 = u8"./Äér/亁井";
    auto file1 = u8"./Äér/fileÇ.txt";
    auto file2 = u8"./Äér/fileÐ.txt";
    auto file3 = u8"./Äér/亁井/file亦.txt";
    EXPECT_TRUE(os::file::delete_dir(dir1)); // possible remnants of previous runs

    EXPECT_TRUE(os::file::create_dir(dir1));
    EXPECT_TRUE(os::file::create_dir(dir2));
    EXPECT_TRUE(os::file::touch(file1));
    EXPECT_TRUE(os::file::touch(file2));
    EXPECT_TRUE(os::file::touch(file3));

    auto files = os::file::list_dir(dir1);
    ASSERT_EQ(files.size(), 2);
    std::vector<unsigned char> data1 = { 0x66, 0x69, 0x6c, 0x65, 0xc3, 0x87, 0x2e, 0x74, 0x78, 0x74, 0x00 }; // fileÇ.txt
    std::vector<unsigned char> data2 = { 0x66, 0x69, 0x6c, 0x65, 0xc3, 0x90, 0x2e, 0x74, 0x78, 0x74, 0x00 }; // fileÐ.txt
    EXPECT_TRUE(memcmp(files.front().data(), data1.data(), data1.size()) == 0 || memcmp(files.front().data(), data2.data(), data2.size()) == 0);
    EXPECT_TRUE(memcmp(files.back().data(), data1.data(), data1.size()) == 0 || memcmp(files.back().data(), data2.data(), data2.size()) == 0);
    EXPECT_STRNE(files.front().data(), files.back().data());

    files = os::file::list_dir(dir2);
    ASSERT_EQ(files.size(), 1);
    EXPECT_TRUE(memcmp(files.front().data(), u8"file亦.txt", 10) == 0);
    EXPECT_STREQ(files.front().data(), files.back().data());

    // cleanup
    EXPECT_TRUE(os::file::delete_dir(dir1));
    EXPECT_FALSE(os::file::is_dir(dir1));
}

TEST_F(TestOsal, is_dir_international)
{
    auto dir1 = u8"./Ðåß";
    auto dir2 = u8"./Ðåß/suß";
    auto file1 = u8"./Ðåß/fileÇ.txt";
    auto file2 = u8"./Ðåß/fileÐ.txt";
    auto file3 = u8"./Ðåß/suß/file3.txt";
    EXPECT_TRUE(os::file::delete_dir(dir1)); // possible remnants of previous runs

    EXPECT_TRUE(os::file::create_dir(dir1));
    EXPECT_TRUE(os::file::create_dir(dir2));
    EXPECT_TRUE(os::file::touch(file1));
    EXPECT_TRUE(os::file::touch(file2));
    EXPECT_TRUE(os::file::touch(file3));

    // "./Ðåß/suß"
    std::vector<unsigned char> dir1_raw{0x2e, 0x2f, 0xc3, 0x90, 0xc3, 0xa5, 0xc3, 0x9f, 0x2f, 0x73, 0x75, 0xc3, 0x9f};
    std::string test_dir(reinterpret_cast<char*>(dir1_raw.data()), dir1_raw.size());

    // "./Ðåß/suß/file3.txt"
    std::vector<unsigned char> dir2_raw{0x2e, 0x2f, 0xc3, 0x90, 0xc3, 0xa5, 0xc3, 0x9f, 0x2f, 0x73, 0x75, 0xc3, 0x9f,
        0x2f, 0x66, 0x69, 0x6c, 0x65, 0x33, 0x2e, 0x74, 0x78, 0x74};
    std::string test_file(reinterpret_cast<char*>(dir2_raw.data()), dir2_raw.size());

    EXPECT_TRUE(os::file::is_dir(test_dir));
    EXPECT_TRUE(os::file::is_reg_file(test_file));
    EXPECT_FALSE(os::file::is_dir(test_file));
    EXPECT_FALSE(os::file::is_reg_file(test_dir));

    // cleanup
    EXPECT_TRUE(os::file::delete_dir(dir1));
    EXPECT_FALSE(os::file::is_dir(dir1));
}

TEST_F(TestOsal, list_dir_null)
{
    auto l = os::file::list_dir(nullptr);
    EXPECT_EQ(l.size(), 0);
}

TEST_F(TestOsal, get_stem)
{
    const char* file = "test";
    EXPECT_STREQ(os::file::get_stem("test").c_str(), file);
    EXPECT_STREQ(os::file::get_stem("test.txt").c_str(), file);
    EXPECT_STREQ(os::file::get_stem("/test").c_str(), file);
    EXPECT_STREQ(os::file::get_stem("/test.txt").c_str(), file);
    EXPECT_STREQ(os::file::get_stem("\\test").c_str(), file);
    EXPECT_STREQ(os::file::get_stem("\\test.txt").c_str(), file);
    EXPECT_STREQ(os::file::get_stem("./test").c_str(), file);
    EXPECT_STREQ(os::file::get_stem("./test.txt").c_str(), file);
    EXPECT_STREQ(os::file::get_stem(".\\test").c_str(), file);
    EXPECT_STREQ(os::file::get_stem(".\\test.txt").c_str(), file);
    EXPECT_STREQ(os::file::get_stem("/some/dir/test").c_str(), file);
    EXPECT_STREQ(os::file::get_stem("/some/dir/test.txt").c_str(), file);
    EXPECT_STREQ(os::file::get_stem("\\some\\dir\\test").c_str(), file);
    EXPECT_STREQ(os::file::get_stem("\\some\\dir\\test.txt").c_str(), file);
    EXPECT_STREQ(os::file::get_stem("./some/dir/test").c_str(), file);
    EXPECT_STREQ(os::file::get_stem("./some/dir/test.txt").c_str(), file);
    EXPECT_STREQ(os::file::get_stem(".\\some\\dir\\test").c_str(), file);
    EXPECT_STREQ(os::file::get_stem(".\\some\\dir\\test.txt").c_str(), file);
    EXPECT_STREQ(os::file::get_stem("/some\\dir/test").c_str(), file);
    EXPECT_STREQ(os::file::get_stem(".\\some/dir\\test.txt").c_str(), file);
    EXPECT_STREQ(os::file::get_stem(u8"./Ðåß.txt").c_str(), "Ðåß");
    EXPECT_STREQ(os::file::get_stem(u8"./Ðåß/fileÇ.txt").c_str(), "fileÇ");
}

TEST_F(TestOsal, get_filename)
{
    const char* file = "test.txt";
    EXPECT_STREQ(os::file::get_filename("test.txt").c_str(), file);
    EXPECT_STREQ(os::file::get_filename("/test.txt").c_str(), file);
    EXPECT_STREQ(os::file::get_filename("\\test.txt").c_str(), file);
    EXPECT_STREQ(os::file::get_filename("./test.txt").c_str(), file);
    EXPECT_STREQ(os::file::get_filename(".\\test.txt").c_str(), file);
    EXPECT_STREQ(os::file::get_filename("/some/dir/test.txt").c_str(), file);
    EXPECT_STREQ(os::file::get_filename("\\some\\dir\\test.txt").c_str(), file);
    EXPECT_STREQ(os::file::get_filename("./some/dir/test.txt").c_str(), file);
    EXPECT_STREQ(os::file::get_filename(".\\some\\dir\\test.txt").c_str(), file);
    EXPECT_STREQ(os::file::get_filename(".\\some/dir\\test.txt").c_str(), file);
    EXPECT_STREQ(os::file::get_filename("/some\\dir/test").c_str(), "test");
    EXPECT_STREQ(os::file::get_filename(u8"./Ðåß.txt").c_str(), "Ðåß.txt");
    EXPECT_STREQ(os::file::get_filename(u8"./Ðåß/fileÇ.txt").c_str(), "fileÇ.txt");
}
