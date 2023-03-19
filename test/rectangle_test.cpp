#include <gtest/gtest.h>

#include "engine/point.hpp"
#include "engine/rectangle.hpp"

namespace devilution {
namespace {

TEST(RectangleTest, Contains_LargerSize)
{
	RectangleOf<uint8_t> rect { { 0, 0 }, SizeOf<uint8_t> { 10, 20 } };
	EXPECT_TRUE(rect.contains(Point(9, 9)));
	EXPECT_FALSE(rect.contains(Point(-1, -1)));
	EXPECT_FALSE(rect.contains(Point(257, 257)));
}

TEST(RectangleTest, Contains_UnsignedRectangle_SignedPointSameSize)
{
	RectangleOf<uint8_t> rect { { 0, 0 }, SizeOf<uint8_t> { 255, 255 } };
	EXPECT_TRUE(rect.contains(PointOf<int8_t>(5, 5)));
	EXPECT_FALSE(rect.contains(PointOf<int8_t>(-1, -1)));
	EXPECT_FALSE(rect.contains(PointOf<int8_t>(-2, -2)));
}

TEST(RectangleTest, Contains_SignedRectangle_UnsignedPointSameSize)
{
	RectangleOf<int8_t> rect { { -10, -10 }, SizeOf<int8_t> { 127, 127 } };
	EXPECT_TRUE(rect.contains(PointOf<uint8_t>(0, 0)));
	EXPECT_FALSE(rect.contains(PointOf<uint8_t>(255, 255)));
}

TEST(RectangleTest, Contains_Rectangle_PointExtent)
{
	RectangleOf<int8_t> rect { { -10, -10 }, PointOf<int8_t> { 112, 112 } };
	EXPECT_FALSE(rect.contains(PointOf<int8_t>(-11, -11)));
	EXPECT_TRUE(rect.contains(PointOf<int8_t>(-10, -10)));
	EXPECT_TRUE(rect.contains(PointOf<int8_t>(0, 0)));
	EXPECT_TRUE(rect.contains(PointOf<int8_t>(112, 112)));
	EXPECT_FALSE(rect.contains(PointOf<int8_t>(113, 113)));
}

} // namespace
} // namespace devilution
