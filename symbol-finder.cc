// Little tool to extract all the types declared and defined in header files

#include <filesystem>
#include <iomanip>
#include <iostream>
#include <memory>
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
using clang::RecursiveASTVisitor;
using clang::SourceLocation;
using clang::SourceManager;
using clang::TypeDecl;

using clang::tooling::ClangTool;
using clang::tooling::CommonOptionsParser;
using clang::tooling::newFrontendActionFactory;

namespace {
class TypeDefinitionVisitor
    : public RecursiveASTVisitor<TypeDefinitionVisitor> {
 public:
  explicit TypeDefinitionVisitor(ASTContext *context)
      : context_(context), cwd_(std::filesystem::current_path().string()) {}

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

    SourceManager &source_manager = context_->getSourceManager();
    SourceLocation loc = node->getLocation();

    if (loc.isInvalid() || source_manager.isInSystemHeader(loc)) {
      return true;
    }

    const std::string raw_name = node->getQualifiedNameAsString();
    std::string_view name = raw_name;
    if (name.find_first_of('<') != std::string_view::npos) {
      // Not interested in templated classes.
      return true;
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
        return true;  // Only interested if filename associated with it.
      }
      // Sometimes, files are provided as relative filenames, sometimes
      // absolute. If the latter, make them relative again.
      if (fileName.starts_with(cwd_)) {
        fileName = fileName.substr(cwd_.length() + 1);
      }
      auto canonical = std::filesystem::path(fileName.begin(), fileName.end())
                           .lexically_normal()
                           .string();
#if 0
      // Full info
      const std::string kind = node->getDeclKindName();
      const int line_number = FullLocation.getSpellingLineNumber();
      std::cout << std::left << std::setw(40) << name << "\t" << kind
                << "\t" << canonical << "\t" << line_number << "\n";
#else
      // Relevant stuff that header-fixer will need
      std::cout << std::left << std::setw(40) << name << " " << canonical
                << "\n";
#endif
    }
    return true;
  }

 private:
  ASTContext *context_;
  const std::string cwd_;
};

class TypeDefinitionConsumer : public clang::ASTConsumer {
 public:
  explicit TypeDefinitionConsumer(ASTContext *context) : Visitor(context) {}

  void HandleTranslationUnit(ASTContext &Context) override {
    Visitor.TraverseDecl(Context.getTranslationUnitDecl());
  }

 private:
  TypeDefinitionVisitor Visitor;
};

class TypeDefinitionAction : public clang::ASTFrontendAction {
 protected:
  std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &ci,
                                                 llvm::StringRef) override {
    return std::make_unique<TypeDefinitionConsumer>(&ci.getASTContext());
  }
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

  CommonOptionsParser &OptionsParser = ExpectedParser.get();
  ClangTool Tool(OptionsParser.getCompilations(),
                 OptionsParser.getSourcePathList());

  return Tool.run(newFrontendActionFactory<TypeDefinitionAction>().get());
}
