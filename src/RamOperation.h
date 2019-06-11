/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2014, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file RamOperation.h
 *
 * Defines the Operation of a relational algebra query.
 *
 ***********************************************************************/

#pragma once

#include "RamCondition.h"
#include "RamExpression.h"
#include "RamNode.h"
#include "RamRelation.h"
#include "RamTypes.h"
#include "Util.h"
#include <cassert>
#include <cstddef>
#include <iosfwd>
#include <memory>
#include <string>
#include <vector>

namespace souffle {

/**
 * Abstract class for a relational algebra operation
 */
class RamOperation : public RamNode {
public:
    /** pretty print with intentation */
    virtual void print(std::ostream& os, int tabpos) const = 0;

    void print(std::ostream& os) const override {
        print(os, 0);
    }

    RamOperation* clone() const override = 0;
};

/**
 * Abstract class for parallel operation
 */
class RamAbstractParallel {};

/**
 * Abstract class for a nesting operations in a loop-nest
 */
class RamNestedOperation : public RamOperation {
public:
    RamNestedOperation(std::unique_ptr<RamOperation> nested, std::string profileText = "")
            : RamOperation(), nestedOperation(std::move(nested)), profileText(std::move(profileText)) {}

    /** Get nested operation */
    RamOperation& getOperation() const {
        assert(nullptr != nestedOperation);
        return *nestedOperation;
    }

    /** Get profile text */
    const std::string& getProfileText() const {
        return profileText;
    }

    void print(std::ostream& os, int tabpos) const override {
        nestedOperation->print(os, tabpos);
    }

    std::vector<const RamNode*> getChildNodes() const override {
        return {nestedOperation.get()};
    }

    void apply(const RamNodeMapper& map) override {
        nestedOperation = map(std::move(nestedOperation));
    }

protected:
    /** Nested operation */
    std::unique_ptr<RamOperation> nestedOperation;

    /** Profile text */
    const std::string profileText;

    bool equal(const RamNode& node) const override {
        assert(nullptr != dynamic_cast<const RamNestedOperation*>(&node));
        const auto& other = static_cast<const RamNestedOperation&>(node);
        return getOperation() == other.getOperation() && getProfileText() == other.getProfileText();
    }
};

/**
 * Abstract class for relation searches and lookups
 */
class RamTupleOperation : public RamNestedOperation {
public:
    RamTupleOperation(int ident, std::unique_ptr<RamOperation> nested, std::string profileText = "")
            : RamNestedOperation(std::move(nested), std::move(profileText)), identifier(ident) {}

    /** Get identifier */
    int getTupleId() const {
        return identifier;
    }

    /** Set identifier */
    void setTupleId(int id) {
        identifier = id;
    }

    std::vector<const RamNode*> getChildNodes() const override {
        return RamNestedOperation::getChildNodes();
    }

protected:
    /** Identifier for the tuple */
    int identifier;

    bool equal(const RamNode& node) const override {
        assert(nullptr != dynamic_cast<const RamTupleOperation*>(&node));
        const auto& other = static_cast<const RamTupleOperation&>(node);
        return RamNestedOperation::equal(other) && getTupleId() == other.getTupleId();
    }
};

/**
 * Abstract class for relation searches
 */
class RamRelationOperation : public RamTupleOperation {
public:
    RamRelationOperation(std::unique_ptr<RamRelationReference> relRef, int ident,
            std::unique_ptr<RamOperation> nested, std::string profileText = "")
            : RamTupleOperation(ident, std::move(nested), std::move(profileText)),
              relationRef(std::move(relRef)) {}

    /** Get search relation */
    const RamRelation& getRelation() const {
        return *relationRef->get();
    }

    void apply(const RamNodeMapper& map) override {
        RamTupleOperation::apply(map);
        relationRef = map(std::move(relationRef));
    }

    std::vector<const RamNode*> getChildNodes() const override {
        auto res = RamTupleOperation::getChildNodes();
        res.push_back(relationRef.get());
        return res;
    }

protected:
    /** Search relation */
    std::unique_ptr<RamRelationReference> relationRef;

    bool equal(const RamNode& node) const override {
        assert(nullptr != dynamic_cast<const RamRelationOperation*>(&node));
        const auto& other = static_cast<const RamRelationOperation&>(node);
        return RamTupleOperation::equal(other) && getRelation() == other.getRelation();
    }
};

/**
 * Relation Scan
 *
 * Iterate all tuples of a relation
 */
class RamScan : public RamRelationOperation {
public:
    RamScan(std::unique_ptr<RamRelationReference> rel, int ident, std::unique_ptr<RamOperation> nested,
            std::string profileText = "")
            : RamRelationOperation(std::move(rel), ident, std::move(nested), std::move(profileText)) {}

    void print(std::ostream& os, int tabpos) const override {
        os << times(" ", tabpos);
        os << "FOR t" << getTupleId();
        os << " IN " << getRelation().getName() << std::endl;
        RamRelationOperation::print(os, tabpos + 1);
    }

    RamScan* clone() const override {
        return new RamScan(std::unique_ptr<RamRelationReference>(relationRef->clone()), getTupleId(),
                std::unique_ptr<RamOperation>(getOperation().clone()), getProfileText());
    }
};

/**
 * Parallel Relation Scan
 *
 * Iterate all tuples of a relation in parallel
 */
class RamParallelScan : public RamScan, public RamAbstractParallel {
public:
    RamParallelScan(std::unique_ptr<RamRelationReference> rel, int ident,
            std::unique_ptr<RamOperation> nested, std::string profileText = "")
            : RamScan(std::move(rel), ident, std::move(nested), profileText) {}

    void print(std::ostream& os, int tabpos) const override {
        os << times(" ", tabpos);
        os << "PARALLEL FOR t" << getTupleId();
        os << " IN " << getRelation().getName() << std::endl;
        RamRelationOperation::print(os, tabpos + 1);
    }

    RamParallelScan* clone() const override {
        return new RamParallelScan(std::unique_ptr<RamRelationReference>(relationRef->clone()), getTupleId(),
                std::unique_ptr<RamOperation>(getOperation().clone()), getProfileText());
    }
};

/**
 * Relation Scan with Index
 *
 * Search for tuples of a relation matching a criteria
 */
class RamIndexOperation : public RamRelationOperation {
public:
    RamIndexOperation(std::unique_ptr<RamRelationReference> r, int ident,
            std::vector<std::unique_ptr<RamExpression>> queryPattern, std::unique_ptr<RamOperation> nested,
            std::string profileText = "")
            : RamRelationOperation(std::move(r), ident, std::move(nested), std::move(profileText)),
              queryPattern(std::move(queryPattern)) {
        assert(getRangePattern().size() == getRelation().getArity());
    }

    /** Get range pattern */
    std::vector<RamExpression*> getRangePattern() const {
        return toPtrVector(queryPattern);
    }

    std::vector<const RamNode*> getChildNodes() const override {
        auto res = RamRelationOperation::getChildNodes();
        for (auto& cur : queryPattern) {
            res.push_back(cur.get());
        }
        return res;
    }

    void apply(const RamNodeMapper& map) override {
        RamRelationOperation::apply(map);
        for (auto& cur : queryPattern) {
            cur = map(std::move(cur));
        }
    }

protected:
    /** Values of index per column of table (if indexable) */
    std::vector<std::unique_ptr<RamExpression>> queryPattern;

    void printIndex(std::ostream& os) const {
        bool first = true;
        for (unsigned int i = 0; i < queryPattern.size(); ++i) {
            if (!isRamUndefValue(queryPattern[i].get())) {
                if (first) {
                    os << " ON INDEX ";
                    first = false;
                } else {
                    os << " AND ";
                }
                os << "t" << getTupleId() << ".";
                os << getRelation().getArg(i) << " = ";
                os << *queryPattern[i];
            }
        }
    }

    bool equal(const RamNode& node) const override {
        assert(nullptr != dynamic_cast<const RamIndexOperation*>(&node));
        const auto& other = static_cast<const RamIndexOperation&>(node);
        return RamRelationOperation::equal(other) && equal_targets(queryPattern, other.queryPattern);
    }
};

/**
 * Relation Scan with Index
 *
 * Search for tuples of a relation matching a criteria
 */
class RamIndexScan : public RamIndexOperation {
public:
    RamIndexScan(std::unique_ptr<RamRelationReference> r, int ident,
            std::vector<std::unique_ptr<RamExpression>> queryPattern, std::unique_ptr<RamOperation> nested,
            std::string profileText = "")
            : RamIndexOperation(std::move(r), ident, std::move(queryPattern), std::move(nested),
                      std::move(profileText)) {}

    void print(std::ostream& os, int tabpos) const override {
        const RamRelation& rel = getRelation();
        os << times(" ", tabpos);
        os << "FOR t" << getTupleId() << " IN ";
        os << rel.getName();
        printIndex(os);
        os << std::endl;
        RamIndexOperation::print(os, tabpos + 1);
    }

    RamIndexScan* clone() const override {
        std::vector<std::unique_ptr<RamExpression>> resQueryPattern(queryPattern.size());
        for (unsigned int i = 0; i < queryPattern.size(); ++i) {
            resQueryPattern[i] = std::unique_ptr<RamExpression>(queryPattern[i]->clone());
        }
        return new RamIndexScan(std::unique_ptr<RamRelationReference>(relationRef->clone()), getTupleId(),
                std::move(resQueryPattern), std::unique_ptr<RamOperation>(getOperation().clone()),
                getProfileText());
    }
};

/**
 * Parallel Relation Scan with Index
 *
 * Search for tuples of a relation matching a criteria
 */
class RamParallelIndexScan : public RamIndexScan, public RamAbstractParallel {
public:
    RamParallelIndexScan(std::unique_ptr<RamRelationReference> rel, int ident,
            std::vector<std::unique_ptr<RamExpression>> queryPattern, std::unique_ptr<RamOperation> nested,
            std::string profileText = "")
            : RamIndexScan(std::move(rel), ident, std::move(queryPattern), std::move(nested), profileText) {}

    void print(std::ostream& os, int tabpos) const override {
        const RamRelation& rel = getRelation();
        os << times(" ", tabpos);
        os << "PARALLEL FOR t" << getTupleId() << " IN ";
        os << rel.getName();
        printIndex(os);
        os << std::endl;
        RamIndexOperation::print(os, tabpos + 1);
    }

    RamParallelIndexScan* clone() const override {
        std::vector<std::unique_ptr<RamExpression>> resQueryPattern(queryPattern.size());
        for (unsigned int i = 0; i < queryPattern.size(); ++i) {
            resQueryPattern[i] = std::unique_ptr<RamExpression>(queryPattern[i]->clone());
        }
        return new RamParallelIndexScan(std::unique_ptr<RamRelationReference>(relationRef->clone()),
                getTupleId(), std::move(resQueryPattern),
                std::unique_ptr<RamOperation>(getOperation().clone()), getProfileText());
    }
};

/**
 * Find a tuple in a relation such that a given condition holds.
 */
class RamChoice : public RamRelationOperation {
public:
    RamChoice(std::unique_ptr<RamRelationReference> rel, size_t ident, std::unique_ptr<RamCondition> cond,
            std::unique_ptr<RamOperation> nested, std::string profileText = "")
            : RamRelationOperation(std::move(rel), ident, std::move(nested), std::move(profileText)),
              condition(std::move(cond)) {}

    /** get condition */
    const RamCondition& getCondition() const {
        assert(condition != nullptr && "condition of choice is a null-pointer");
        return *condition;
    }

    void print(std::ostream& os, int tabpos) const override {
        os << times(" ", tabpos);
        os << "CHOICE t" << getTupleId();
        os << " IN " << getRelation().getName();
        os << " WHERE " << getCondition();
        os << std::endl;
        RamRelationOperation::print(os, tabpos + 1);
    }

    void apply(const RamNodeMapper& map) override {
        RamRelationOperation::apply(map);
        condition = map(std::move(condition));
    }

    RamChoice* clone() const override {
        return new RamChoice(std::unique_ptr<RamRelationReference>(relationRef->clone()), getTupleId(),
                std::unique_ptr<RamCondition>(condition->clone()),
                std::unique_ptr<RamOperation>(getOperation().clone()), getProfileText());
    }

    std::vector<const RamNode*> getChildNodes() const override {
        return {nestedOperation.get(), relationRef.get(), condition.get()};
    }

protected:
    std::unique_ptr<RamCondition> condition;

    bool equal(const RamNode& node) const override {
        assert(nullptr != dynamic_cast<const RamChoice*>(&node));
        const auto& other = static_cast<const RamChoice&>(node);
        return RamRelationOperation::equal(other) && getCondition() == other.getCondition();
    }
};

/**
 * Find a tuple in a relation such that a given condition holds.
 */
class RamParallelChoice : public RamChoice, public RamAbstractParallel {
public:
    RamParallelChoice(std::unique_ptr<RamRelationReference> rel, size_t ident,
            std::unique_ptr<RamCondition> cond, std::unique_ptr<RamOperation> nested,
            std::string profileText = "")
            : RamChoice(std::move(rel), ident, std::move(cond), std::move(nested), profileText) {}

    void print(std::ostream& os, int tabpos) const override {
        os << times(" ", tabpos);
        os << "PARALLEL CHOICE t" << getTupleId();
        os << " IN " << getRelation().getName();
        os << " WHERE " << getCondition();
        os << std::endl;
        RamRelationOperation::print(os, tabpos + 1);
    }

    RamParallelChoice* clone() const override {
        return new RamParallelChoice(std::unique_ptr<RamRelationReference>(relationRef->clone()),
                getTupleId(), std::unique_ptr<RamCondition>(condition->clone()),
                std::unique_ptr<RamOperation>(getOperation().clone()), getProfileText());
    }
};

/**
 * Use an index to find a tuple in a relation such that a given condition holds.
 */
class RamIndexChoice : public RamIndexOperation {
public:
    RamIndexChoice(std::unique_ptr<RamRelationReference> r, int ident, std::unique_ptr<RamCondition> cond,
            std::vector<std::unique_ptr<RamExpression>> queryPattern, std::unique_ptr<RamOperation> nested,
            std::string profileText = "")
            : RamIndexOperation(std::move(r), ident, std::move(queryPattern), std::move(nested),
                      std::move(profileText)),
              condition(std::move(cond)) {
        assert(getRangePattern().size() == getRelation().getArity());
    }

    /** get condition */
    const RamCondition& getCondition() const {
        assert(condition != nullptr && "condition of index-choice is a null-pointer");
        return *condition;
    }

    void print(std::ostream& os, int tabpos) const override {
        const RamRelation& rel = getRelation();
        os << times(" ", tabpos);
        os << "CHOICE " << rel.getName() << " AS t" << getTupleId();
        printIndex(os);
        os << " WHERE " << getCondition();
        os << std::endl;
        RamIndexOperation::print(os, tabpos + 1);
    }

    void apply(const RamNodeMapper& map) override {
        RamRelationOperation::apply(map);
        for (auto& cur : queryPattern) {
            cur = map(std::move(cur));
        }
        condition = map(std::move(condition));
    }

    std::vector<const RamNode*> getChildNodes() const override {
        auto res = RamIndexOperation::getChildNodes();
        res.push_back(condition.get());
        return res;
    }

    RamIndexChoice* clone() const override {
        std::vector<std::unique_ptr<RamExpression>> resQueryPattern(queryPattern.size());
        for (size_t i = 0; i < queryPattern.size(); ++i) {
            resQueryPattern[i] = std::unique_ptr<RamExpression>(queryPattern[i]->clone());
        }
        RamIndexChoice* res = new RamIndexChoice(std::unique_ptr<RamRelationReference>(relationRef->clone()),
                getTupleId(), std::unique_ptr<RamCondition>(condition->clone()), std::move(resQueryPattern),
                std::unique_ptr<RamOperation>(getOperation().clone()), getProfileText());
        return res;
    }

protected:
    std::unique_ptr<RamCondition> condition;

    bool equal(const RamNode& node) const override {
        assert(nullptr != dynamic_cast<const RamIndexChoice*>(&node));
        const auto& other = static_cast<const RamIndexChoice&>(node);
        return RamRelationOperation::equal(other) && equal_targets(queryPattern, other.queryPattern) &&
               getCondition() == other.getCondition();
    }
};

/**
 * Use an index to find a tuple in a relation such that a given condition holds.
 */
class RamParallelIndexChoice : public RamIndexChoice, public RamAbstractParallel {
public:
    RamParallelIndexChoice(std::unique_ptr<RamRelationReference> r, int ident,
            std::unique_ptr<RamCondition> cond, std::vector<std::unique_ptr<RamExpression>> queryPattern,
            std::unique_ptr<RamOperation> nested, std::string profileText = "")
            : RamIndexChoice(std::move(r), ident, std::move(cond), std::move(queryPattern), std::move(nested),
                      profileText) {}

    void print(std::ostream& os, int tabpos) const override {
        const RamRelation& rel = getRelation();
        os << times(" ", tabpos);
        os << "PARALLEL CHOICE " << rel.getName() << " AS t" << getTupleId();
        printIndex(os);
        os << " WHERE " << getCondition();
        os << std::endl;
        RamIndexOperation::print(os, tabpos + 1);
    }

    RamParallelIndexChoice* clone() const override {
        std::vector<std::unique_ptr<RamExpression>> resQueryPattern(queryPattern.size());
        for (size_t i = 0; i < queryPattern.size(); ++i) {
            resQueryPattern[i] = std::unique_ptr<RamExpression>(queryPattern[i]->clone());
        }
        RamParallelIndexChoice* res = new RamParallelIndexChoice(
                std::unique_ptr<RamRelationReference>(relationRef->clone()), getTupleId(),
                std::unique_ptr<RamCondition>(condition->clone()), std::move(resQueryPattern),
                std::unique_ptr<RamOperation>(getOperation().clone()), getProfileText());
        return res;
    }
};

/** Types of aggregation functions */
enum AggregateFunction { MAX, MIN, COUNT, SUM };

/**
 * Abstract class for aggregation
 */
class RamAbstractAggregate {
public:
    RamAbstractAggregate(AggregateFunction fun, std::unique_ptr<RamExpression> expression,
            std::unique_ptr<RamCondition> condition)
            : function(fun), expression(std::move(expression)), condition(std::move(condition)) {}

    /** Get condition */
    const RamCondition& getCondition() const {
        assert(condition != nullptr && "Condition of aggregate is a null-pointer");
        return *condition;
    }

    /** Get aggregation function */
    AggregateFunction getFunction() const {
        return function;
    }

    /** Get target expression */
    const RamExpression& getExpression() const {
        assert(expression != nullptr && "Expression of aggregate is a null-pointer");
        return *expression;
    }

    std::vector<const RamNode*> getChildNodes() const {
        return {expression.get(), condition.get()};
    }

    void print(std::ostream& os, int tabpos) const {
        switch (function) {
            case MIN:
                os << "MIN ";
                break;
            case MAX:
                os << "MAX ";
                break;
            case COUNT:
                os << "COUNT ";
                break;
            case SUM:
                os << "SUM ";
                break;
        }
        if (function != COUNT) {
            os << *expression << " ";
        }
    }

protected:
    /** Aggregation function */
    AggregateFunction function;

    /** Aggregation expression */
    std::unique_ptr<RamExpression> expression;

    /** Aggregation tuple condition */
    std::unique_ptr<RamCondition> condition;

    bool equal(const RamNode& node) const {
        assert(nullptr != dynamic_cast<const RamAbstractAggregate*>(&node));
        const auto& other = dynamic_cast<const RamAbstractAggregate*>(&node);
        return getCondition() == other->getCondition() && getFunction() == other->getFunction() &&
               getExpression() == other->getExpression();
    }
};

/**
 * Aggregation
 */
class RamAggregate : public RamRelationOperation, public RamAbstractAggregate {
public:
    RamAggregate(std::unique_ptr<RamOperation> nested, AggregateFunction fun,
            std::unique_ptr<RamRelationReference> relRef, std::unique_ptr<RamExpression> expression,
            std::unique_ptr<RamCondition> condition, int ident)
            : RamRelationOperation(std::move(relRef), ident, std::move(nested)),
              RamAbstractAggregate(fun, std::move(expression), std::move(condition)) {}

    void print(std::ostream& os, int tabpos) const override {
        os << times(" ", tabpos);
        os << "t" << getTupleId() << ".0=";
        RamAbstractAggregate::print(os, tabpos);
        os << " FOR ALL t" << getTupleId() << " ∈ " << getRelation().getName();
        if (!isRamTrue(condition.get())) {
            os << " WHERE " << getCondition();
        }
        os << std::endl;
        RamRelationOperation::print(os, tabpos + 1);
    }

    std::vector<const RamNode*> getChildNodes() const override {
        auto res = RamRelationOperation::getChildNodes();
        auto children = RamAbstractAggregate::getChildNodes();
        res.insert(res.end(), children.begin(), children.end());
        return res;
    }

    RamAggregate* clone() const override {
        return new RamAggregate(std::unique_ptr<RamOperation>(getOperation().clone()), function,
                std::unique_ptr<RamRelationReference>(relationRef->clone()),
                std::unique_ptr<RamExpression>(expression->clone()),
                std::unique_ptr<RamCondition>(condition->clone()), getTupleId());
    }

    void apply(const RamNodeMapper& map) override {
        RamRelationOperation::apply(map);
        condition = map(std::move(condition));
        expression = map(std::move(expression));
    }

protected:
    bool equal(const RamNode& node) const override {
        const auto& other = static_cast<const RamAggregate&>(node);
        return RamRelationOperation::equal(other) && RamAbstractAggregate::equal(node);
    }
};

/**
 * Index Aggregation
 */
class RamIndexAggregate : public RamIndexOperation, public RamAbstractAggregate {
public:
    RamIndexAggregate(std::unique_ptr<RamOperation> nested, AggregateFunction fun,
            std::unique_ptr<RamRelationReference> relRef, std::unique_ptr<RamExpression> expression,
            std::unique_ptr<RamCondition> condition, std::vector<std::unique_ptr<RamExpression>> queryPattern,
            int ident)
            : RamIndexOperation(std::move(relRef), ident, std::move(queryPattern), std::move(nested)),
              RamAbstractAggregate(fun, std::move(expression), std::move(condition)) {}

    void print(std::ostream& os, int tabpos) const override {
        os << times(" ", tabpos);
        os << "t" << getTupleId() << ".0=";
        RamAbstractAggregate::print(os, tabpos);
        os << " SEARCH t" << getTupleId() << " ∈ " << getRelation().getName();
        printIndex(os);
        if (!isRamTrue(condition.get())) {
            os << " WHERE " << getCondition();
        }
        os << std::endl;
        RamIndexOperation::print(os, tabpos + 1);
    }

    std::vector<const RamNode*> getChildNodes() const override {
        auto res = RamIndexOperation::getChildNodes();
        auto children = RamAbstractAggregate::getChildNodes();
        res.insert(res.end(), children.begin(), children.end());
        return res;
    }

    RamIndexAggregate* clone() const override {
        std::vector<std::unique_ptr<RamExpression>> pattern;
        for (auto const& e : queryPattern) {
            pattern.push_back(std::unique_ptr<RamExpression>(e->clone()));
        }
        return new RamIndexAggregate(std::unique_ptr<RamOperation>(getOperation().clone()), function,
                std::unique_ptr<RamRelationReference>(relationRef->clone()),
                std::unique_ptr<RamExpression>(expression->clone()),
                std::unique_ptr<RamCondition>(condition->clone()), std::move(pattern), getTupleId());
    }

    void apply(const RamNodeMapper& map) override {
        RamIndexOperation::apply(map);
        condition = map(std::move(condition));
        expression = map(std::move(expression));
    }

protected:
    bool equal(const RamNode& node) const override {
        const auto& other = static_cast<const RamAggregate&>(node);
        return RamIndexOperation::equal(other) && RamAbstractAggregate::equal(node);
    }
};

/**
 * Record lookup
 */
class RamUnpackRecord : public RamTupleOperation {
public:
    RamUnpackRecord(std::unique_ptr<RamOperation> nested, int ident, std::unique_ptr<RamExpression> expr,
            size_t arity)
            : RamTupleOperation(ident, std::move(nested)), expression(std::move(expr)), arity(arity) {}

    /** Get expression */
    const RamExpression& getExpression() const {
        assert(expression != nullptr && "Expression of unpack-record is a null-pointer");
        return *expression;
    }

    /** Get arity */
    std::size_t getArity() const {
        return arity;
    }

    std::vector<const RamNode*> getChildNodes() const override {
        auto res = RamTupleOperation::getChildNodes();
        res.push_back(expression.get());
        return res;
    }

    void print(std::ostream& os, int tabpos) const override {
        os << times(" ", tabpos) << "UNPACK " << getExpression() << " INTO t" << getTupleId() << std::endl;
        RamTupleOperation::print(os, tabpos + 1);
    }

    RamUnpackRecord* clone() const override {
        return new RamUnpackRecord(std::unique_ptr<RamOperation>(getOperation().clone()), getTupleId(),
                std::unique_ptr<RamExpression>(getExpression().clone()), arity);
    }

    void apply(const RamNodeMapper& map) override {
        RamTupleOperation::apply(map);
        expression = map(std::move(expression));
    }

protected:
    /** Expression for record reference */
    std::unique_ptr<RamExpression> expression;

    /** Arity of the unpacked tuple */
    const size_t arity;

    bool equal(const RamNode& node) const override {
        assert(nullptr != dynamic_cast<const RamUnpackRecord*>(&node));
        const auto& other = static_cast<const RamUnpackRecord&>(node);
        return RamTupleOperation::equal(other) && getExpression() == other.getExpression() &&
               getArity() == other.getArity();
    }
};

/**
 * Abstract conditional statement
 */
class RamAbstractConditional : public RamNestedOperation {
public:
    RamAbstractConditional(std::unique_ptr<RamCondition> cond, std::unique_ptr<RamOperation> nested,
            std::string profileText = "")
            : RamNestedOperation(std::move(nested), std::move(profileText)), condition(std::move(cond)) {}

    /** Get condition */
    const RamCondition& getCondition() const {
        assert(condition != nullptr && "condition of conditional operation is a null-pointer");
        return *condition;
    }

    std::vector<const RamNode*> getChildNodes() const override {
        auto res = RamNestedOperation::getChildNodes();
        res.push_back(condition.get());
        return res;
    }

    void apply(const RamNodeMapper& map) override {
        RamNestedOperation::apply(map);
        condition = map(std::move(condition));
    }

protected:
    /** Condition */
    std::unique_ptr<RamCondition> condition;

    bool equal(const RamNode& node) const override {
        assert(nullptr != dynamic_cast<const RamAbstractConditional*>(&node));
        const auto& other = static_cast<const RamAbstractConditional&>(node);
        return RamNestedOperation::equal(node) && getCondition() == other.getCondition();
    }
};

/**
 * Filter statement
 */
class RamFilter : public RamAbstractConditional {
public:
    RamFilter(std::unique_ptr<RamCondition> cond, std::unique_ptr<RamOperation> nested,
            std::string profileText = "")
            : RamAbstractConditional(std::move(cond), std::move(nested), std::move(profileText)) {}

    void print(std::ostream& os, int tabpos) const override {
        os << times(" ", tabpos);
        os << "IF " << getCondition() << std::endl;
        RamNestedOperation::print(os, tabpos + 1);
    }

    RamFilter* clone() const override {
        return new RamFilter(std::unique_ptr<RamCondition>(condition->clone()),
                std::unique_ptr<RamOperation>(getOperation().clone()), getProfileText());
    }

protected:
    bool equal(const RamNode& node) const override {
        return RamAbstractConditional::equal(node);
    }
};

/**
 * Break statement
 */
class RamBreak : public RamAbstractConditional {
public:
    RamBreak(std::unique_ptr<RamCondition> cond, std::unique_ptr<RamOperation> nested,
            std::string profileText = "")
            : RamAbstractConditional(std::move(cond), std::move(nested), std::move(profileText)) {}

    void print(std::ostream& os, int tabpos) const override {
        os << times(" ", tabpos);
        os << "IF " << getCondition() << " BREAK" << std::endl;
        RamNestedOperation::print(os, tabpos);
    }

    RamBreak* clone() const override {
        return new RamBreak(std::unique_ptr<RamCondition>(condition->clone()),
                std::unique_ptr<RamOperation>(getOperation().clone()), getProfileText());
    }

protected:
    bool equal(const RamNode& node) const override {
        return RamAbstractConditional::equal(node);
    }
};

/** Projection */
class RamProject : public RamOperation {
public:
    RamProject(std::unique_ptr<RamRelationReference> relRef,
            std::vector<std::unique_ptr<RamExpression>> expressions)
            : RamOperation(), relationRef(std::move(relRef)), expressions(std::move(expressions)) {}

    /** Get relation */
    const RamRelation& getRelation() const {
        return *relationRef->get();
    }

    /** Get expressions */
    std::vector<RamExpression*> getValues() const {
        return toPtrVector(expressions);
    }

    void print(std::ostream& os, int tabpos) const override {
        os << times(" ", tabpos);
        os << "PROJECT (" << join(expressions, ", ", print_deref<std::unique_ptr<RamExpression>>())
           << ") INTO " << getRelation().getName() << std::endl;
    }

    std::vector<const RamNode*> getChildNodes() const override {
        std::vector<const RamNode*> res;
        res.push_back(relationRef.get());
        for (const auto& cur : expressions) {
            res.push_back(cur.get());
        }
        return res;
    }

    RamProject* clone() const override {
        std::vector<std::unique_ptr<RamExpression>> newValues;
        for (auto& cur : expressions) {
            newValues.emplace_back(cur->clone());
        }
        return new RamProject(
                std::unique_ptr<RamRelationReference>(relationRef->clone()), std::move(newValues));
    }

    void apply(const RamNodeMapper& map) override {
        relationRef = map(std::move(relationRef));
        for (auto& cur : expressions) {
            cur = map(std::move(cur));
        }
    }

protected:
    /** Relation */
    std::unique_ptr<RamRelationReference> relationRef;

    /* Values for projection */
    std::vector<std::unique_ptr<RamExpression>> expressions;

    bool equal(const RamNode& node) const override {
        assert(nullptr != dynamic_cast<const RamProject*>(&node));
        const auto& other = static_cast<const RamProject&>(node);
        return getRelation() == other.getRelation() && equal_targets(expressions, other.expressions);
    }
};

/** A statement for returning from a ram subroutine */
class RamSubroutineReturnValue : public RamOperation {
public:
    RamSubroutineReturnValue(std::vector<std::unique_ptr<RamExpression>> vals)
            : RamOperation(), expressions(std::move(vals)) {}

    void print(std::ostream& os, int tabpos) const override {
        os << times(" ", tabpos);
        os << "RETURN (";
        for (auto val : getValues()) {
            val->print(os);
            if (val != *(getValues().end() - 1)) {
                os << ", ";
            }
        }
        os << ")" << std::endl;
    }

    std::vector<RamExpression*> getValues() const {
        return toPtrVector(expressions);
    }

    std::vector<const RamNode*> getChildNodes() const override {
        std::vector<const RamNode*> res;
        for (const auto& cur : expressions) {
            res.push_back(cur.get());
        }
        return res;
    }

    RamSubroutineReturnValue* clone() const override {
        std::vector<std::unique_ptr<RamExpression>> newValues;
        for (auto& cur : expressions) {
            newValues.emplace_back(cur->clone());
        }
        return new RamSubroutineReturnValue(std::move(newValues));
    }

    void apply(const RamNodeMapper& map) override {
        for (auto& cur : expressions) {
            cur = map(std::move(cur));
        }
    }

protected:
    /** Return expressions */
    std::vector<std::unique_ptr<RamExpression>> expressions;

    bool equal(const RamNode& node) const override {
        assert(nullptr != dynamic_cast<const RamSubroutineReturnValue*>(&node));
        const auto& other = static_cast<const RamSubroutineReturnValue&>(node);
        return equal_targets(expressions, other.expressions);
    }
};

}  // namespace souffle
