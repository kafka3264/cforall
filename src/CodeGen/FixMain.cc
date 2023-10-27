//
// Cforall Version 1.0.0 Copyright (C) 2015 University of Waterloo
//
// The contents of this file are covered under the licence agreement in the
// file "LICENCE" distributed with Cforall.
//
// FixMain.cc --
//
// Author           : Thierry Delisle
// Created On       : Thr Jan 12 14:11:09 2017
// Last Modified By :
// Last Modified On :
// Update Count     : 0
//

#include "FixMain.h"

#include <cassert>                 // for assert, assertf
#include <fstream>                 // for operator<<, basic_ostream::operator<<
#include <list>                    // for list
#include <string>                  // for operator<<

#include "AST/Decl.hpp"
#include "AST/Pass.hpp"
#include "AST/Type.hpp"
#include "AST/Vector.hpp"
#include "Common/PassVisitor.h"
#include "Common/SemanticError.h"  // for SemanticError
#include "CodeGen/GenType.h"       // for GenType
#include "SynTree/Declaration.h"   // for FunctionDecl, operator<<
#include "SynTree/Type.h"          // for FunctionType
#include "SymTab/Mangler.h"

namespace CodeGen {

namespace {

struct FindMainCore {
	FunctionDecl * main_signature = nullptr;

	void previsit( FunctionDecl * decl ) {
		if ( FixMain::isMain( decl ) ) {
			if ( main_signature ) {
				SemanticError( decl, "Multiple definition of main routine\n" );
			}
			main_signature = decl;
		}
	}
};

struct FindMainCore_new {
	ast::FunctionDecl const * main_declaration = nullptr;

	void previsit( ast::FunctionDecl const * decl ) {
		if ( FixMain::isMain( decl ) ) {
			if ( main_declaration ) {
				SemanticError( decl, "Multiple definition of main routine\n" );
			}
			main_declaration = decl;
		}
	}
};

std::string genTypeAt( const ast::vector<ast::Type> & types, size_t at ) {
	return genType( types[at], "", Options( false, false, false, false ) );
}

}

	template<typename container>
	std::string genTypeAt(const container& p, size_t idx) {
		return genType((*std::next(p.begin(), idx))->get_type(), "");
	}

	void FixMain::fix( std::list< Declaration * > & translationUnit,
			std::ostream &os, const char* bootloader_filename ) {
		PassVisitor< FindMainCore > main_finder;
		acceptAll( translationUnit, main_finder );
		FunctionDecl * main_signature = main_finder.pass.main_signature;

		if( main_signature ) {
			os << "static inline int invoke_main(int argc, char* argv[], char* envp[]) { (void)argc; (void)argv; (void)envp; return ";
			main_signature->mangleName = SymTab::Mangler::mangle(main_signature);

			os << main_signature->get_scopedMangleName() << "(";
			const auto& params = main_signature->get_functionType()->get_parameters();
			switch(params.size()) {
				case 3: os << "(" << genTypeAt(params, 0) << ")argc, (" << genTypeAt(params, 1) << ")argv, (" << genTypeAt(params, 2) << ")envp"; break;
				case 2: os << "(" << genTypeAt(params, 0) << ")argc, (" << genTypeAt(params, 1) << ")argv"; break;
				case 0: break;
				default : assert(false);
			}
			os << "); }\n";

			std::ifstream bootloader(bootloader_filename, std::ios::in);
			assertf( bootloader.is_open(), "cannot open bootloader.c\n" );
			os << bootloader.rdbuf();
		}
	}

namespace {

ast::ObjectDecl * makeIntObj(){
	return new ast::ObjectDecl( CodeLocation(), "",
		new ast::BasicType( ast::BasicType::SignedInt ) );
}

ast::ObjectDecl * makeCharStarStarObj() {
	return new ast::ObjectDecl( CodeLocation(), "",
		new ast::PointerType(
			new ast::PointerType(
				new ast::BasicType( ast::BasicType::Char ) ) ) );
}

std::string getMangledNameOfMain(
		ast::vector<ast::DeclWithType> && params, ast::ArgumentFlag isVarArgs ) {
	ast::ptr<ast::FunctionDecl> decl = new ast::FunctionDecl(
		CodeLocation(),
		"main",
		ast::vector<ast::TypeDecl>(),
		ast::vector<ast::DeclWithType>(),
		std::move( params ),
		{ makeIntObj() },
		nullptr,
		ast::Storage::Classes(),
		ast::Linkage::Spec(),
		ast::vector<ast::Attribute>(),
		ast::Function::Specs(),
		isVarArgs
	);
	return Mangle::mangle( decl.get() );
}

std::string getMangledNameOf0ParameterMain() {
	return getMangledNameOfMain( {}, ast::VariableArgs );
}

std::string getMangledNameOf2ParameterMain() {
	return getMangledNameOfMain( {
		makeIntObj(),
		makeCharStarStarObj(),
	}, ast::FixedArgs );
}

bool is_main( const std::string & mangled_name ) {
	// This breaks if you move it out of the function.
	static const std::string mangled_mains[] = {
		getMangledNameOf0ParameterMain(),
		getMangledNameOf2ParameterMain(),
		//getMangledNameOf3ParameterMain(),
	};

	for ( auto main_name : mangled_mains ) {
		if ( main_name == mangled_name ) return true;
	}
	return false;
}

} // namespace

bool FixMain::isMain( FunctionDecl * decl ) {
	if ( std::string("main") != decl->name ) {
		return false;
	}
	return is_main( SymTab::Mangler::mangle( decl, true, true ) );
}

bool FixMain::isMain( const ast::FunctionDecl * decl ) {
	if ( std::string("main") != decl->name ) {
		return false;
	}
	return is_main( Mangle::mangle( decl, Mangle::Type ) );
}

void FixMain::fix( ast::TranslationUnit & translationUnit,
		std::ostream &os, const char * bootloader_filename ) {

	ast::Pass<FindMainCore_new> main_finder;
	ast::accept_all( translationUnit, main_finder );
	if ( nullptr == main_finder.core.main_declaration ) return;

	ast::FunctionDecl * main_declaration =
		ast::mutate( main_finder.core.main_declaration );

	main_declaration->mangleName = Mangle::mangle( main_declaration );

	os << "static inline int invoke_main(int argc, char* argv[], char* envp[]) { (void)argc; (void)argv; (void)envp; return ";
	os << main_declaration->scopedMangleName() << "(";
	const auto& params = main_declaration->type->params;
	switch ( params.size() ) {
		case 3: os << "(" << genTypeAt(params, 0) << ")argc, (" << genTypeAt(params, 1) << ")argv, (" << genTypeAt(params, 2) << ")envp"; break;
		case 2: os << "(" << genTypeAt(params, 0) << ")argc, (" << genTypeAt(params, 1) << ")argv"; break;
		case 0: break;
		default : assert(false);
	}
	os << "); }\n";

	std::ifstream bootloader( bootloader_filename, std::ios::in );
	assertf( bootloader.is_open(), "cannot open bootloader.c\n" );
	os << bootloader.rdbuf();
}

} // namespace CodeGen
