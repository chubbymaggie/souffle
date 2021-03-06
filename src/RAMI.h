/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2019, The Souffle Developers. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file RAMI.h
 *
 * Declares the RAMI (RamInterpreter) class for executing RAM programs.
 *
 ***********************************************************************/

#pragma once

#include "RAMIContext.h"
#include "RAMIInterface.h"
#include "RAMIRelation.h"
#include "RamCondition.h"
#include "RamRelation.h"
#include "RamStatement.h"
#include "RamTranslationUnit.h"
#include "RamTypes.h"
#include "RelationRepresentation.h"

#include <atomic>
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <map>
#include <string>
#include <utility>
#include <vector>
#include <dlfcn.h>

namespace souffle {

class RMAIProgInterface;
class RamOperation;
class RamExpression;
class SymbolTable;

/**
 * Interpreter executing a RAM translation unit
 */

class RAMI : public RAMIInterface {
public:
    RAMI(RamTranslationUnit& tUnit) : RAMIInterface(tUnit) {}
    ~RAMI() {
        for (auto& x : environment) {
            delete x.second;
        }
    }

    /** Execute main program */
    void executeMain() override;

    /* Execute subroutine */
    void executeSubroutine(const std::string& name, const std::vector<RamDomain>& arguments,
            std::vector<RamDomain>& returnValues, std::vector<bool>& returnErrors) override;

protected:
    /** Evaluate value */
    RamDomain evalExpr(const RamExpression& value, const RAMIContext& ctxt = RAMIContext());

    /** Evaluate operation */
    void evalOp(const RamOperation& op, const RAMIContext& ctxt = RAMIContext());

    /** Evaluate conditions */
    bool evalCond(const RamCondition& cond, const RAMIContext& ctxt = RAMIContext());

    /** Evaluate statement */
    void evalStmt(const RamStatement& stmt, const RAMIContext& ctxt = RAMIContext());

    /** Get symbol table */
    SymbolTable& getSymbolTable() {
        return translationUnit.getSymbolTable();
    }

    /** Get counter */
    int getCounter() const {
        return counter;
    }

    /** Get Iteration Number */
    size_t getIterationNumber() const {
        return iteration;
    }

    /** Increment counter */
    int incCounter() {
        return counter++;
    }

    /** Increment iteration number */
    void incIterationNumber() {
        iteration++;
    }

    /** Reset iteration number */
    void resetIterationNumber() {
        iteration = 0;
    }

    void createRelation(const RamRelation& id, const MinIndexSelection* orderSet) {
        RAMIRelation* res = nullptr;
        assert(environment.find(id.getName()) == environment.end());
        if (id.getRepresentation() == RelationRepresentation::EQREL) {
            res = new RAMIEqRelation(id.getArity(), orderSet, id.getName());
        } else {
            res = new RAMIRelation(id.getArity(), orderSet, id.getName());
        }
        environment[id.getName()] = res;
    }

    /** Get relation */
    RAMIRelation& getRelation(const std::string& name) {
        // look up relation
        auto pos = environment.find(name);
        assert(pos != environment.end());
        return *pos->second;
    }

    /** Get relation */
    inline RAMIRelation& getRelation(const RamRelation& id) {
        return getRelation(id.getName());
    }

    /** Drop relation */
    void dropRelation(const RamRelation& id) {
        RAMIRelation& rel = getRelation(id);
        environment.erase(id.getName());
        delete &rel;
    }

    /** Swap relation */
    void swapRelation(const RamRelation& ramRel1, const RamRelation& ramRel2) {
        RAMIRelation* rel1 = &getRelation(ramRel1);
        RAMIRelation* rel2 = &getRelation(ramRel2);
        environment[ramRel1.getName()] = rel2;
        environment[ramRel2.getName()] = rel1;
    }

private:
    friend RAMIProgInterface;

    /** relation environment type */
    using relation_map = std::map<std::string, RAMIRelation*>;

    /** Get relation map */
    virtual std::map<std::string, RAMIRelation*>& getRelationMap() override {
        return environment;
    }

    /** counters for atom profiling */
    std::map<std::string, std::map<size_t, size_t>> frequencies;

    /** counters for non-existence checks */
    std::map<std::string, std::atomic<size_t>> reads;

    /** counter for $ operator */
    int counter = 0;

    /** iteration number (in a fix-point calculation) */
    size_t iteration = 0;

    /** Relation Environment */
    relation_map environment;
};

}  // end of namespace souffle
