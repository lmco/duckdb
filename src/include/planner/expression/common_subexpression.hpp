//===----------------------------------------------------------------------===//
//                         DuckDB
//
// planner/expression/common_subexpression.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "planner/expression.hpp"

namespace duckdb {
//! Represents a CommonSubExpression, this is only generated by the optimizers. CSEs cannot be serialized, deserialized
//! or copied.
class CommonSubExpression : public Expression {
public:
	CommonSubExpression(unique_ptr<Expression> child, string alias);
	CommonSubExpression(Expression *child, string alias);

	//! The child of the CSE
	Expression *child;
	//! The owned child of the CSE (if any)
	unique_ptr<Expression> owned_child;

public:
	bool IsScalar() const override {
		return false;
	}
	bool IsFoldable() const override {
		return false;
	}

	string ToString() const override;

	bool Equals(const BaseExpression *other) const override;

	unique_ptr<Expression> Copy() override;
};
} // namespace duckdb
