/******************************************************************************
 * Copyright (c) 2017 Philipp Schubert.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Philipp Schubert and others
 *****************************************************************************/

/*
 * InterMonoSolver.h
 *
 *  Created on: 19.06.2017
 *      Author: philipp
 */

#ifndef PHASAR_PHASARLLVM_MONO_SOLVER_INTERMONOSOLVER_H_
#define PHASAR_PHASARLLVM_MONO_SOLVER_INTERMONOSOLVER_H_

#include <deque>
#include <iosfwd>
#include <iostream>
#include <utility>
#include <vector>

#include <phasar/Config/ContainerConfiguration.h>
#include <phasar/PhasarLLVM/DataFlowSolver/Mono/Contexts/CallStringCTX.h>
#include <phasar/PhasarLLVM/DataFlowSolver/Mono/InterMonoProblem.h>
#include <phasar/Utils/LLVMShorthands.h>

namespace psr {

template <typename N, typename D, typename M, typename T, typename V,
          typename I, unsigned K>
class InterMonoSolver {
public:
  using ProblemTy = InterMonoProblem<N, D, M, T, V, I>;

protected:
  InterMonoProblem<N, D, M, T, V, I> &IMProblem;
  std::deque<std::pair<N, N>> Worklist;
  MonoMap<N, MonoMap<CallStringCTX<D, N, K>, MonoSet<D>>> Analysis;
  MonoSet<M> AddedFunctions;
  const I *ICF;

  void initialize() {
    for (auto &seed : IMProblem.initialSeeds()) {
      std::vector<std::pair<N, N>> edges =
          ICF->getAllControlFlowEdges(ICF->getFunctionOf(seed.first));
      Worklist.insert(Worklist.begin(), edges.begin(), edges.end());
      // Initialize with empty context and empty data-flow set such that the
      // flow functions are at least called once per instruction
      for (auto &edge : edges) {
        Analysis[edge.first][CallStringCTX<D, N, K>()].insert({});
      }
      // Initialize last
      if (!edges.empty()) {
        Analysis[edges.back().second][CallStringCTX<D, N, K>()].insert({});
      }
      // Additionally, insert the initial seeds
      Analysis[seed.first][CallStringCTX<D, N, K>()].insert(seed.second.begin(),
                                                            seed.second.end());
    }
  }

  bool isIntraEdge(std::pair<N, N> edge) {
    return ICF->getFunctionOf(edge.first) == ICF->getFunctionOf(edge.second);
  }

  bool isCallEdge(std::pair<N, N> edge) {
    return !isIntraEdge(edge) && ICF->isCallStmt(edge.first);
  }

  bool isReturnEdge(std::pair<N, N> edge) {
    return !isIntraEdge(edge) && ICF->isExitStmt(edge.first);
  }

  void printWorkList() {
    std::cout << "CURRENT WORKLIST:" << std::endl;
    for (auto Entry : Worklist) {
      std::cout << llvmIRToString(Entry.first) << " ---> "
                << llvmIRToString(Entry.second) << std::endl;
    }
    std::cout << "-----------------" << std::endl;
  }

  void printMonoSet(const MonoSet<D> &S) {
    std::cout << "SET CONTENTS:\n{ ";
    for (auto Entry : S) {
      std::cout << llvmIRToString(Entry) << ", ";
    }
    std::cout << "}" << std::endl;
  }

  void addCalleesToWorklist(std::pair<N, N> edge) {
    auto src = edge.first;
    auto dst = edge.second;
    // Add inter- and intra-edges of callee(s)
    for (auto callee : ICF->getCalleesOfCallAt(src)) {
      if (AddedFunctions.count(callee)) {
        break;
      }
      AddedFunctions.insert(callee);
      // Add call edge(s)
      for (auto startPoint : ICF->getStartPointsOf(callee)) {
        Worklist.push_back({src, startPoint});
      }
      // Add intra edges of callee
      std::vector<std::pair<N, N>> edges = ICF->getAllControlFlowEdges(callee);
      Worklist.insert(Worklist.begin(), edges.begin(), edges.end());
      // Initialize with empty context and empty data-flow set such that the
      // flow functions are at least called once per instruction
      for (auto &edge : edges) {
        Analysis[edge.first][CallStringCTX<D, N, K>()].insert({});
      }
      // Initialize last
      if (!edges.empty()) {
        Analysis[edges.back().second][CallStringCTX<D, N, K>()].insert({});
      }
      // Add return edge(s)
      for (auto ret : ICF->getExitPointsOf(callee)) {
        for (auto retSite : ICF->getReturnSitesOfCallAt(src)) {
          Worklist.push_back({ret, retSite});
        }
      }
    }
    // The (intra-procedural) call-to-return edge of the caller is already in
    // the worklist
  }

  void addToWorklist(std::pair<N, N> edge) {
    auto src = edge.first;
    auto dst = edge.second;
    Worklist.push_back({src, dst});
    // add intra-procedural edges again
    for (auto nprimeprime : ICF->getSuccsOf(dst)) {
      Worklist.push_back({dst, nprimeprime});
    }
    // add inter-procedural call edges again
    if (ICF->isCallStmt(dst)) {
      for (auto callee : ICF->getCalleesOfCallAt(dst)) {
        for (auto startPoint : ICF->getStartPointsOf(callee)) {
          Worklist.push_back({dst, startPoint});
        }
      }
    }
    // add inter-procedural return edges again
    if (ICF->isExitStmt(dst)) {
      for (auto caller : ICF->getCallersOf(ICF->getFunctionOf(dst))) {
        for (auto nprimeprime : ICF->getSuccsOf(caller)) {
          Worklist.push_back({dst, nprimeprime});
        }
      }
    }
  }

public:
  InterMonoSolver(InterMonoProblem<N, D, M, T, V, I> &IMP)
      : IMProblem(IMP), ICF(IMP.getICFG()) {}
  InterMonoSolver(const InterMonoSolver &) = delete;
  InterMonoSolver &operator=(const InterMonoSolver &) = delete;
  InterMonoSolver(InterMonoSolver &&) = delete;
  InterMonoSolver &operator=(InterMonoSolver &&) = delete;
  virtual ~InterMonoSolver() = default;

  MonoMap<N, MonoMap<CallStringCTX<D, N, K>, MonoSet<D>>> getAnalysis() {
    return Analysis;
  }

  virtual void solve() {
    initialize();
    while (!Worklist.empty()) {
      std::pair<N, N> edge = Worklist.front();
      Worklist.pop_front();
      auto src = edge.first;
      auto dst = edge.second;
      if (ICF->isCallStmt(src)) {
        addCalleesToWorklist(edge);
      }
      // Compute the data-flow facts using the respective flow function
      MonoMap<CallStringCTX<D, N, K>, MonoSet<D>> Out;
      if (ICF->isCallStmt(src)) {
        // Handle call and call-to-ret flow
        if (!isIntraEdge(edge)) {
          // Handle call flow
          for (auto &[CTX, Facts] : Analysis[src]) {
            auto CTXAdd(CTX);
            CTXAdd.push_back(src);
            Out[CTXAdd] = IMProblem.callFlow(src, ICF->getFunctionOf(dst),
                                             Analysis[src][CTX]);
            bool flowfactsstabilized =
                IMProblem.sqSubSetEqual(Out[CTXAdd], Analysis[dst][CTXAdd]);
            if (!flowfactsstabilized) {
              Analysis[dst][CTXAdd] =
                  IMProblem.join(Analysis[dst][CTXAdd], Out[CTXAdd]);
              addToWorklist({src, dst});
            }
          }
        } else {
          // Handle call-to-ret flow
          for (auto &[CTX, Facts] : Analysis[src]) {
            // call-to-ret flow does not modify contexts
            Out[CTX] = IMProblem.callToRetFlow(
                src, dst, ICF->getCalleesOfCallAt(src), Analysis[src][CTX]);
            bool flowfactsstabilized =
                IMProblem.sqSubSetEqual(Out[CTX], Analysis[dst][CTX]);
            if (!flowfactsstabilized) {
              Analysis[dst][CTX] = IMProblem.join(Analysis[dst][CTX], Out[CTX]);
              addToWorklist({src, dst});
            }
          }
        }
      } else if (ICF->isExitStmt(src)) {
        // Handle return flow
        for (auto &[CTX, Facts] : Analysis[src]) {
          auto CTXRm(CTX);
          // we need to use several call- and retsites if the context is empty
          std::set<N> callsites;
          std::set<N> retsites;
          // handle empty context
          if (CTX.empty()) {
            callsites = ICF->getCallersOf(ICF->getFunctionOf(src));
          } else {
            // handle context containing at least one element
            callsites.insert(CTXRm.pop_back());
          }
          // retrieve the possible return sites for each call
          for (auto callsite : callsites) {
            auto retsitesPerCall = ICF->getReturnSitesOfCallAt(callsite);
            retsites.insert(retsitesPerCall.begin(), retsitesPerCall.end());
          }
          for (auto callsite : callsites) {
            auto retFactsPerCall =
                IMProblem.returnFlow(callsite, ICF->getFunctionOf(src), src,
                                     dst, Analysis[src][CTX]);
            Out[CTXRm].insert(retFactsPerCall.begin(), retFactsPerCall.end());
          }
          for (auto retsite : retsites) {
            bool flowfactsstabilized =
                IMProblem.sqSubSetEqual(Out[CTXRm], Analysis[retsite][CTXRm]);
            if (!flowfactsstabilized) {
              Analysis[dst][CTXRm] =
                  IMProblem.join(Analysis[retsite][CTXRm], Out[CTXRm]);
              addToWorklist({src, retsite});
            }
          }
        }
      } else {
        // Handle normal flow
        for (auto &[CTX, Facts] : Analysis[src]) {
          Out[CTX] = IMProblem.normalFlow(src, Analysis[src][CTX]);
          // Check if data-flow facts have changed and if so, add edge(s) to
          // worklist again.
          bool flowfactsstabilized =
              IMProblem.sqSubSetEqual(Out[CTX], Analysis[dst][CTX]);
          if (!flowfactsstabilized) {
            Analysis[dst][CTX] = IMProblem.join(Analysis[dst][CTX], Out[CTX]);
            addToWorklist({src, dst});
          }
        }
      }
    }
    if (PhasarConfig::VariablesMap().count("emit-raw-results")) {
      dumpResults();
    }
  }

  MonoSet<D> getResultsAt(N n) {
    MonoSet<D> Result;
    for (auto &[CTX, Facts] : Analysis[n]) {
      Result.insert(Facts.begin(), Facts.end());
    }
    return Result;
  }

  void dumpResults() {
    std::cout << "======= DUMP LLVM-INTER-MONOTONE-SOLVER RESULTS =======\n";
    for (auto &entry : this->Analysis) {
      std::cout << "Instruction:\n" << this->IMProblem.NtoString(entry.first);
      std::cout << "\nFacts:\n";
      if (entry.second.empty()) {
        std::cout << "\tEMPTY\n";
      } else {
        for (auto &context : entry.second) {
          std::cout << context.first << '\n';
          if (context.second.empty()) {
            std::cout << "\tEMPTY\n";
          } else {
            for (auto &fact : context.second) {
              std::cout << this->IMProblem.DtoString(fact);
            }
          }
        }
      }
      std::cout << '\n';
    }
  }
};

} // namespace psr

#endif