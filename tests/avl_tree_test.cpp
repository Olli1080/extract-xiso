#include <algorithm>
#include <cmath>
#include <compare>
#include <numeric>
#include <random>
#include <vector>

#include <gtest/gtest.h>

#include "xiso/avl_tree.hpp"

namespace
{

struct IntOrder
{
	std::strong_ordering operator()(int lhs, int rhs) const noexcept { return lhs <=> rhs; }
};

using IntTree = xiso::AvlTree<int, IntOrder>;

std::vector<int> preorder(const IntTree& tree)
{
	std::vector<int> values;
	tree.for_each_preorder([&](const IntTree::Node& node) { values.push_back(node.value); });
	return values;
}

/// Height of a subtree, failing the test if any node is out of balance.
int checked_height(const IntTree::Node* node)
{
	if (!node) return 0;
	const int left = checked_height(node->left.get());
	const int right = checked_height(node->right.get());
	EXPECT_LE(std::abs(left - right), 1) << "unbalanced at " << node->value;
	if (node->left) EXPECT_LT(node->left->value, node->value);
	if (node->right) EXPECT_GT(node->right->value, node->value);
	return 1 + std::max(left, right);
}

} // namespace

TEST(AvlTree, StartsEmpty)
{
	IntTree tree;
	EXPECT_TRUE(tree.empty());
	EXPECT_EQ(tree.height(), 0);
	EXPECT_TRUE(preorder(tree).empty());
}

TEST(AvlTree, AscendingInsertsGiveAPerfectTree)
{
	IntTree tree;
	for (int i = 1; i <= 7; ++i) ASSERT_NE(tree.insert(i), nullptr);

	EXPECT_EQ(preorder(tree), (std::vector<int>{4, 2, 1, 3, 6, 5, 7}));
	EXPECT_EQ(tree.height(), 3);
}

TEST(AvlTree, DoubleRotations)
{
	IntTree left_right;
	for (int value : {3, 1, 2}) left_right.insert(value);
	EXPECT_EQ(preorder(left_right), (std::vector<int>{2, 1, 3}));

	IntTree right_left;
	for (int value : {1, 3, 2}) right_left.insert(value);
	EXPECT_EQ(preorder(right_left), (std::vector<int>{2, 1, 3}));
}

TEST(AvlTree, RejectsDuplicates)
{
	IntTree tree;
	ASSERT_NE(tree.insert(5), nullptr);
	ASSERT_NE(tree.insert(3), nullptr);
	EXPECT_EQ(tree.insert(5), nullptr);
	EXPECT_EQ(preorder(tree).size(), 2u);
}

TEST(AvlTree, StaysBalancedForAnyInsertionOrder)
{
	std::vector<int> values(2000);
	std::iota(values.begin(), values.end(), 0);

	std::mt19937 random(1234);
	for (int round = 0; round < 5; ++round)
	{
		std::shuffle(values.begin(), values.end(), random);
		IntTree tree;
		for (int value : values) ASSERT_NE(tree.insert(value), nullptr);

		EXPECT_EQ(checked_height(tree.root()), tree.height());
		EXPECT_LE(tree.height(), 1.45 * std::log2(values.size() + 2)); // AVL height bound
		EXPECT_EQ(preorder(tree).size(), values.size());
	}
}

TEST(AvlTree, ReturnedPointersSurviveRebalancing)
{
	IntTree tree;
	std::vector<int*> stored;
	for (int i = 0; i < 100; ++i) stored.push_back(tree.insert(i));

	for (int i = 0; i < 100; ++i) EXPECT_EQ(*stored[i], i);
}
