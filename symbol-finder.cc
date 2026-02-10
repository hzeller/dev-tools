// Little tool to extract all the types declared and defined in header files

#include <filesystem>
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>
#include <set>
#include <string>
#include <string_view>

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Basic/SourceLocation.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"

using clang::ASTConsumer;
using clang::ASTContext;
using clang::CompilerInstance;
using clang::Decl;
using clang::FullSourceLoc;
using clang::FunctionDecl;
using clang::NamedDecl;
using clang::RecursiveASTVisitor;
using clang::SourceLocation;
using clang::SourceManager;
using clang::TypeDecl;

using clang::tooling::ClangTool;
using clang::tooling::CommonOptionsParser;
using clang::tooling::newFrontendActionFactory;

namespace {
using FindingPrinter =
    std::function<void(std::string_view symbol, std::string_view filename)>;

class SymbolDefinitionVisitor
    : public RecursiveASTVisitor<SymbolDefinitionVisitor> {
 public:
  explicit SymbolDefinitionVisitor(ASTContext *context,
                                   const FindingPrinter &printer)
      : context_(context),
        cwd_(std::filesystem::current_path().string()),
        printer_(printer) {}

  void Emit(NamedDecl *node) {
    SourceManager &source_manager = context_->getSourceManager();
    SourceLocation loc = node->getLocation();

    if (loc.isInvalid() || source_manager.isInSystemHeader(loc)) {
      return;
    }

    const std::string raw_name = node->getQualifiedNameAsString();
    std::string_view name = raw_name;
    if (name.find_first_of('<') != std::string_view::npos) {
      // Not interested in templated classes.
      return;
    }

    if (name.find('(') != std::string_view::npos) {
      // Some sort of unnamed stuff or anonymous namespace
      return;
    }

    // Soemtimes we get that these are allegedly in the 'std::' namespace.
    // This is probably due to some bogus 'using namespace std;' somewhere.
    // Let's remove (we already filtered out system headers)
    if (name.length() > 4 && name.substr(0, 5) == "std::") {
      name = name.substr(5);
    }

    FullSourceLoc FullLocation = context_->getFullLoc(loc);
    if (FullLocation.isValid()) {
      llvm::StringRef fileName = source_manager.getFilename(FullLocation);
      if (fileName.empty()) {
        return;  // Only interested if filename associated with it.
      }
      // Sometimes, files are provided as relative filenames, sometimes
      // absolute. If the latter, make them relative again.
      if (fileName.starts_with(cwd_)) {
        fileName = fileName.substr(cwd_.length() + 1);
      }
      auto canonical = std::filesystem::path(fileName.begin(), fileName.end())
                           .lexically_normal()
                           .string();
      printer_(name, canonical);
#if 0
      // Full info
      const std::string kind = node->getDeclKindName();
      const int line_number = FullLocation.getSpellingLineNumber();
      std::cout << std::left << std::setw(40) << name << "\t" << kind
                << "\t" << canonical << "\t" << line_number << "\n";
#endif
    }
  }

  // TypeDecl covers TagDecl (Struct/Class/Enum) and TypedefNameDecl
  bool VisitTypeDecl(TypeDecl *node) {
    // Only interested in some.
    switch (node->getKind()) {
      case Decl::Record:     // C struct/union
      case Decl::CXXRecord:  // C++ class/struct
      case Decl::Enum: {     // Enum
        // ... but we're only interested in definitions, not forward dcls.
        auto *tag_decl = llvm::cast<clang::TagDecl>(node);
        if (!tag_decl->isThisDeclarationADefinition()) return true;
        break;
      }
      case Decl::Typedef:    // typedef
      case Decl::TypeAlias:  // using X = Y
        break;               // We care about these
      default:
        return true;  // Skip everything else (TemplateTypeParm, etc.)
    }

    Emit(node);
    return true;
  }

  bool VisitFunctionDecl(FunctionDecl *node) {
    // Check if it's a free-standing function (not a class member)
    if (llvm::isa<clang::CXXMethodDecl>(node)) {
      return true;  // not interested in method declaration
    }
    Emit(node);
    return true;  // Return true to continue the traversal
  }

 private:
  ASTContext *context_;
  const std::string cwd_;
  FindingPrinter printer_;
};

class SymbolDefinitionConsumer : public clang::ASTConsumer {
 public:
  explicit SymbolDefinitionConsumer(ASTContext *context,
                                    const FindingPrinter &printer)
      : visitor_(context, printer) {}

  void HandleTranslationUnit(ASTContext &Context) override {
    visitor_.TraverseDecl(Context.getTranslationUnitDecl());
  }

 private:
  SymbolDefinitionVisitor visitor_;
};

class SymbolDefinitionAction : public clang::ASTFrontendAction {
 protected:
  std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &ci,
                                                 llvm::StringRef) override {
    return std::make_unique<SymbolDefinitionConsumer>(
        &ci.getASTContext(),
        [](std::string_view symbol, std::string_view filename) {
          PrintUnique(symbol, filename);
        });
  }

  static void PrintUnique(std::string_view symbol, std::string_view filename) {
    // The SymbolDefinitionAction is created multiple times. Have a local
    // copy (TODO: do we have to worry about threads ?)
    static auto *uniqifier = new std::set<UniqOutput>();
    auto to_print = UniqOutput{.name = std::string(symbol),
                               .filename = std::string(filename)};
    if (uniqifier->insert(to_print).second) {
      std::cout << std::left << std::setw(40) << symbol << " " << filename
                << "\n";
    }
  }

 private:
  struct UniqOutput {
    std::string name;
    std::string filename;
    auto operator<=>(const UniqOutput &) const = default;
  };
};

}  // namespace

int main(int argc, const char **argv) {
  llvm::cl::OptionCategory typeFinderCategory("type-finder options");
  auto ExpectedParser =
      CommonOptionsParser::create(argc, argv, typeFinderCategory);
  if (!ExpectedParser) {
    llvm::errs() << ExpectedParser.takeError();
    return 1;
  }

  CommonOptionsParser &options_parser = ExpectedParser.get();
  ClangTool tool(options_parser.getCompilations(),
                 options_parser.getSourcePathList());

  return tool.run(newFrontendActionFactory<SymbolDefinitionAction>().get());
}
