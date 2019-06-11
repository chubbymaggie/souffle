/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2019, The Souffle Developers. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file LVM.h
 *
 * Declares the bytecode interpreter class
 *
 ***********************************************************************/

#pragma once

#include "LVMCode.h"
#include "LVMContext.h"
#include "LVMGenerator.h"
#include "LVMInterface.h"
#include "LVMRelation.h"
#include "Logger.h"
#include "RamTranslationUnit.h"
#include "RamTypes.h"
#include "RelationRepresentation.h"

#include <atomic>
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <map>
#include <stack>
#include <string>
#include <utility>
#include <vector>
#include <dlfcn.h>

namespace souffle {
class LVMProgInterface;

/**
 * Bytecode Interpreter executing a RAM translation unit.
 *
 * RAM is transferred into an equivalent Bytecode Representation (LVMCode) by LVMGenerator.
 * LVM then directly execute the program by interpreting the LVMCode.
 * The LVMCode will be cached in the memory to save time when repeatedly execution is needed.
 */
class LVM : public LVMInterface {
public:
    LVM(RamTranslationUnit& tUnit) : LVMInterface(tUnit) {
        // Construct mapping from relation Name to RamRelation node in RAM tree.
        // This will later be used for fast lookup during RamRelationCreate in order to retrieve
        // minIndexSet from a given relation.
        visitDepthFirst(*translationUnit.getProgram(), [&](const RamRelation& node) {
            relNameToNode.insert(std::make_pair(node.getName(), &node));
        });
    }

    virtual ~LVM() {
        for (auto* timer : timers) {
            delete timer;
        }
    }

    /** Execute the main program */
    void executeMain() override;

    /** Clean the cache of main Program */
    void resetMainProgram() {
        mainProgram.reset();
    }

    /** Clean the cache of subroutine */
    void resetSubroutine(const std::string& name) {
        subroutines.erase(name);
    }

    /** Execute the subroutine */
    void executeSubroutine(const std::string& name, const std::vector<RamDomain>& arguments,
            std::vector<RamDomain>& returnValues, std::vector<bool>& returnErrors) override {
        LVMContext ctxt;
        ctxt.setReturnValues(returnValues);
        ctxt.setReturnErrors(returnErrors);
        ctxt.setArguments(arguments);

        if (subroutines.find(name) != subroutines.cend()) {
            execute(subroutines.at(name), ctxt);
        } else {
            // Parse and cache the program
            LVMGenerator generator(translationUnit.getSymbolTable(),
                    translationUnit.getProgram()->getSubroutine(name), *isa, relationEncoder);
            subroutines.emplace(std::make_pair(name, generator.getCodeStream()));
            execute(subroutines.at(name), ctxt);
        }
    }

    /** Print out the instruction stream */
    void printMain() {
        if (mainProgram.get() == nullptr) {
            LVMGenerator generator(translationUnit.getSymbolTable(), *translationUnit.getProgram()->getMain(),
                    *isa, relationEncoder);
            mainProgram = generator.getCodeStream();
        }
        mainProgram->print();
    }

protected:
    using index_set =
            btree_multiset<const RamDomain*, LVMIndex::comparator, std::allocator<const RamDomain*>, 512>;

    /** Insert Logger */
    void insertTimerAt(size_t index, Logger* timer) {
        if (index >= timers.size()) {
            timers.resize((index + 1), nullptr);
        }
        timers[index] = timer;
    }

    /** Stop and destroy logger */
    void stopTimerAt(size_t index) {
        assert(index < timers.size());
        delete timers[index];
        timers[index] = nullptr;
    }

    /** Get symbol table */
    SymbolTable& getSymbolTable() {
        return translationUnit.getSymbolTable();
    }

    /** Get Counter */
    int getCounter() {
        return counter;
    }

    /** Increment counter */
    int incCounter() {
        return counter++;
    }

    /** Increment iteration number */
    void incIterationNumber() {
        iteration++;
    }

    /** Get Iteration Number */
    size_t getIterationNumber() const {
        return iteration;
    }

    /** Reset iteration number */
    void resetIterationNumber() {
        iteration = 0;
    }

    /** Get a relation */
    LVMRelation* getRelation(size_t id) {
        return environment[id].get();
    }

    /** Drop relation */
    void dropRelation(size_t id) {
        environment[id].reset(nullptr);
    }

    /** Swap relation */
    void swapRelation(size_t relAId, size_t relBId) {
        environment[relAId].swap(environment[relBId]);
    }

    /** Lookup iterator, resize the iterator pool if necessary */
    std::pair<index_set::iterator, index_set::iterator>& lookUpIterator(size_t idx) {
        if (idx >= iteratorPool.size()) {
            iteratorPool.resize(idx + 1);
        }
        return iteratorPool[idx];
    }

    /** Obtain the search columns */
    SearchSignature getSearchSignature(const std::string& patterns, size_t arity) {
        SearchSignature res = 0;
        for (size_t i = 0; i < arity; ++i) {
            if (patterns[i] == 'V') {
                res |= (1 << i);
            }
        }
        return res;
    }

private:
    friend LVMProgInterface;

    /** Execute given program
     *
     * @param ip the instruction pointer start position, default is 0.
     * */
    void execute(std::unique_ptr<LVMCode>& codeStream, LVMContext& ctxt, size_t ip = 0);

    /** subroutines */
    std::map<std::string, std::unique_ptr<LVMCode>> subroutines;

    /** Main program */
    std::unique_ptr<LVMCode> mainProgram = nullptr;

    /** counters for atom profiling */
    std::map<std::string, std::map<size_t, size_t>> frequencies;

    /** counters for non-existence check */
    std::map<std::string, std::atomic<size_t>> reads;

    /** List of iters for indexScan operation */
    std::vector<std::pair<index_set::iterator, index_set::iterator>> iteratorPool;

    /** Hash map from relationName to RamRelationNode in RAM */
    std::unordered_map<std::string, const RamRelation*> relNameToNode;

    /** stratum */
    size_t level = 0;

    /** List of loggers for logtimer */
    std::vector<Logger*> timers;

    /** counter for $ operator */
    int counter = 0;

    /** iteration number (in a fix-point calculation) */
    size_t iteration = 0;

    /** Dynamic library for user-defined functors */
    void* dll = nullptr;

    /** Relation Encode */
    RelationEncoder relationEncoder;
};

}  // end of namespace souffle
