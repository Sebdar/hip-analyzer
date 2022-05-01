/** \file cfg_instrumentation.cpp
 * \brief
 *
 * \author Sébastien Darche <sebastien.darche@polymtl.ca>
 */

#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/Analysis/CFG.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Tooling/Core/Replacement.h"

#include "clang/Lex/Lexer.h"

#include <memory>
#include <sstream>
#include <string>

// Temporary
#include <iostream>

using namespace clang;
using namespace clang::ast_matchers;

namespace hip {

std::string generateBlockCode(unsigned int id) {
    std::stringstream ss;
    ss << "/* BB " << id << " */" << '\n';
    return ss.str();
}

/** \class KernelCfgInstrumenter
 * \brief AST Matcher callback to instrument CFG blocks. To be run first
 */
class KernelCfgInstrumenter : public MatchFinder::MatchCallback {
  public:
    KernelCfgInstrumenter(const std::string& kernel_name,
                          const std::string& output_filename)
        : name(kernel_name), output_file(output_filename, error_code) {}

    virtual void run(const MatchFinder::MatchResult& Result) {
        auto lang_opt = Result.Context->getLangOpts();

        clang::tooling::Replacements reps;
        rewriter.setSourceMgr(*Result.SourceManager, lang_opt);

        if (const auto* match =
                Result.Nodes.getNodeAs<clang::FunctionDecl>(name)) {
            match->dump();
            auto body = match->getBody();
            auto cfg = CFG::buildCFG(match, body, Result.Context,
                                     clang::CFG::BuildOptions());
            cfg->dump(lang_opt, true);

            // Add replacement for counter declaration

            // Add replacement for counter initialization

            // Print First elements
            for (auto block : *cfg.get()) {
                auto id = block->getBlockID();

                std::cout << "\nBlock " << id << '\n';

                auto first = block->front();
                auto first_statement = first.getAs<clang::CFGStmt>();

                if (first_statement.hasValue()) {
                    auto stmt = first_statement->getStmt();
                    auto begin_loc = stmt->getBeginLoc();
                    begin_loc.dump(*Result.SourceManager);

                    // Unused, might cause issues when entering a conditional
                    // block
                    /*
                    auto rep_loc = clang::Lexer::getLocForEndOfToken(
                        begin_loc, 0, *Result.SourceManager, lang_opt);
                    rep_loc.dump(*Result.SourceManager);
                    */

                    stmt->dumpColor();

                    // Create replacement
                    clang::tooling::Replacement rep(*Result.SourceManager,
                                                    stmt->getBeginLoc(), 0,
                                                    generateBlockCode(id));

                    std::cout << rep.toString();
                    auto err = reps.add(rep);
                    if (err) {
                        throw std::runtime_error(
                            "Incompatible edit encountered");
                    }
                }
            }
            // Check if replacements need to be applied

            if (!reps.empty()) {
                for (auto rep : reps) {
                    rep.apply(rewriter);
                }
                // rewriter.overwriteChangedFiles(); // Rewrites the input file

                rewriter.getEditBuffer(Result.SourceManager->getMainFileID())
                    .write(output_file);
                output_file.close();
            }

            // kernel = match;
        }
    }

    clang::FunctionDecl* getKernel() { return kernel; }

  private:
    std::error_code error_code;
    const std::string name;
    clang::FunctionDecl* kernel = nullptr;
    clang::Rewriter rewriter;
    llvm::raw_fd_ostream output_file;
};

/** \class KernelBaseInstrumenter
 * \brief AST Matcher callback to add instrumentation basics (extra params &
 * local variables)
 */
class KernelBaseInstrumenter : public MatchFinder::MatchCallback {
  public:
    KernelBaseInstrumenter(const std::string& kernel_name)
        : name(kernel_name) {}

    virtual void run(const MatchFinder::MatchResult& Result) {
        auto lang_opt = Result.Context->getLangOpts();

        clang::tooling::Replacements reps;
        rewriter.setSourceMgr(*Result.SourceManager, lang_opt);

        if (const auto* match =
                Result.Nodes.getNodeAs<clang::FunctionDecl>(name)) {
            match->dump();
        }
    }

  private:
    std::string name;
    clang::Rewriter rewriter;
    // llvm::raw_fd_ostream output_file;
};

/** \brief AST matchers
 */
clang::ast_matchers::DeclarationMatcher
kernelMatcher(const std::string& kernel_name) {
    return functionDecl(hasName(kernel_name)).bind(kernel_name);
}

/** \brief MatchCallbacks
 */
std::unique_ptr<MatchFinder::MatchCallback>
makeCfgInstrumenter(const std::string& kernel, const std::string& output_file) {
    return std::make_unique<KernelCfgInstrumenter>(kernel, output_file);
}

std::unique_ptr<MatchFinder::MatchCallback>
makeBaseInstrumenter(const std::string& kernel) {
    return std::make_unique<KernelBaseInstrumenter>(kernel);
}

} // namespace hip