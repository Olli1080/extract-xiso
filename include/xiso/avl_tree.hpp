// Copyright (c) 2003 in <in@fishtank.com> - see LICENSE.TXT
#pragma once

#include <compare>
#include <memory>
#include <utility>

namespace xiso
{

/// AVL tree that owns its values.
///
/// The Xbox is picky about how directory tables are laid out, so the exact tree shape produced by the historical
/// insertion algorithm matters: it determines the on-disk image. Do not swap this for std::map (red-black tree).
///
/// \tparam T       stored value
/// \tparam Compare callable returning std::strong_ordering for two T
template <typename T, typename Compare> class AvlTree
{
	enum class Skew
	{
		none,
		left,
		right
	};

public:
	struct Node
	{
		T value;
		std::unique_ptr<Node> left;
		std::unique_ptr<Node> right;

	private:
		friend class AvlTree;
		Skew skew = Skew::none;
	};

	AvlTree() = default;
	explicit AvlTree(Compare compare) : compare_(std::move(compare)) {}

	[[nodiscard]] bool empty() const noexcept { return !root_; }
	[[nodiscard]] const Node* root() const noexcept { return root_.get(); }
	[[nodiscard]] Node* root() noexcept { return root_.get(); }

	/// Inserts \p value. Returns a pointer to the stored value, or nullptr if an equal value already exists.
	/// The pointer stays valid for the lifetime of the tree (rebalancing relinks nodes, it never moves values).
	T* insert(T value)
	{
		auto node = std::make_unique<Node>();
		node->value = std::move(value);
		T* stored = &node->value;
		return insert_node(root_, node) == Growth::duplicate ? nullptr : stored;
	}

	/// Visits every node parent first, then the left subtree, then the right subtree.
	template <typename Visitor> void for_each_preorder(Visitor&& visitor) { visit_preorder(root_.get(), visitor); }

	template <typename Visitor> void for_each_preorder(Visitor&& visitor) const
	{
		visit_preorder(root_.get(), visitor);
	}

	/// Height of the tree (0 for an empty tree).
	[[nodiscard]] int height() const noexcept { return height_of(root_.get()); }

private:
	using NodePtr = std::unique_ptr<Node>;

	enum class Growth
	{
		unchanged,
		grown,
		duplicate
	};

	template <typename N, typename Visitor> static void visit_preorder(N* node, Visitor& visitor)
	{
		if (!node) return;
		visitor(*node);
		visit_preorder(node->left.get(), visitor);
		visit_preorder(node->right.get(), visitor);
	}

	static int height_of(const Node* node) noexcept
	{
		if (!node) return 0;
		const int left = height_of(node->left.get());
		const int right = height_of(node->right.get());
		return 1 + (left > right ? left : right);
	}

	Growth insert_node(NodePtr& root, NodePtr& node)
	{
		if (!root)
		{
			root = std::move(node);
			return Growth::grown;
		}

		const std::strong_ordering order = compare_(node->value, root->value);
		if (order < 0)
		{
			const Growth result = insert_node(root->left, node);
			return result == Growth::grown ? left_grown(root) : result;
		}
		if (order > 0)
		{
			const Growth result = insert_node(root->right, node);
			return result == Growth::grown ? right_grown(root) : result;
		}
		return Growth::duplicate;
	}

	static Growth left_grown(NodePtr& root)
	{
		switch (root->skew)
		{
		case Skew::left:
			if (root->left->skew == Skew::left)
			{
				root->skew = root->left->skew = Skew::none;
				rotate_right(root);
			}
			else
			{
				switch (root->left->right->skew)
				{
				case Skew::left:
					root->skew = Skew::right;
					root->left->skew = Skew::none;
					break;
				case Skew::right:
					root->skew = Skew::none;
					root->left->skew = Skew::left;
					break;
				default:
					root->skew = Skew::none;
					root->left->skew = Skew::none;
					break;
				}
				root->left->right->skew = Skew::none;
				rotate_left(root->left);
				rotate_right(root);
			}
			return Growth::unchanged;

		case Skew::right:
			root->skew = Skew::none;
			return Growth::unchanged;

		default:
			root->skew = Skew::left;
			return Growth::grown;
		}
	}

	static Growth right_grown(NodePtr& root)
	{
		switch (root->skew)
		{
		case Skew::left:
			root->skew = Skew::none;
			return Growth::unchanged;

		case Skew::right:
			if (root->right->skew == Skew::right)
			{
				root->skew = root->right->skew = Skew::none;
				rotate_left(root);
			}
			else
			{
				switch (root->right->left->skew)
				{
				case Skew::left:
					root->skew = Skew::none;
					root->right->skew = Skew::right;
					break;
				case Skew::right:
					root->skew = Skew::left;
					root->right->skew = Skew::none;
					break;
				default:
					root->skew = Skew::none;
					root->right->skew = Skew::none;
					break;
				}
				root->right->left->skew = Skew::none;
				rotate_right(root->right);
				rotate_left(root);
			}
			return Growth::unchanged;

		default:
			root->skew = Skew::right;
			return Growth::grown;
		}
	}

	static void rotate_left(NodePtr& root)
	{
		NodePtr pivot = std::move(root);
		root = std::move(pivot->right);
		pivot->right = std::move(root->left);
		root->left = std::move(pivot);
	}

	static void rotate_right(NodePtr& root)
	{
		NodePtr pivot = std::move(root);
		root = std::move(pivot->left);
		pivot->left = std::move(root->right);
		root->right = std::move(pivot);
	}

	Compare compare_{};
	NodePtr root_;
};

} // namespace xiso
