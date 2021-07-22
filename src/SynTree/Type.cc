//
// Cforall Version 1.0.0 Copyright (C) 2015 University of Waterloo
//
// The contents of this file are covered under the licence agreement in the
// file "LICENCE" distributed with Cforall.
//
// Type.cc --
//
// Author           : Richard C. Bilson
// Created On       : Mon May 18 07:44:20 2015
// Last Modified By : Andrew Beach
// Last Modified On : Wed Jul 14 15:47:00 2021
// Update Count     : 50
//
#include "Type.h"

#include "Attribute.h"                // for Attribute
#include "Common/utility.h"           // for cloneAll, deleteAll, printAll
#include "InitTweak/InitTweak.h"      // for getPointerBase
#include "SynTree/BaseSyntaxNode.h"   // for BaseSyntaxNode
#include "SynTree/Declaration.h"      // for TypeDecl
#include "SynTree/TypeSubstitution.h" // for TypeSubstitution

using namespace std;

// GENERATED START, DO NOT EDIT
// GENERATED BY BasicTypes-gen.cc
const char * BasicType::typeNames[] = {
	"_Bool",
	"char",
	"signed char",
	"unsigned char",
	"signed short int",
	"unsigned short int",
	"signed int",
	"unsigned int",
	"signed long int",
	"unsigned long int",
	"signed long long int",
	"unsigned long long int",
	"__int128",
	"unsigned __int128",
	"_Float16",
	"_Float16 _Complex",
	"_Float32",
	"_Float32 _Complex",
	"float",
	"float _Complex",
	"_Float32x",
	"_Float32x _Complex",
	"_Float64",
	"_Float64 _Complex",
	"double",
	"double _Complex",
	"_Float64x",
	"_Float64x _Complex",
	"__float80",
	"_Float128",
	"_Float128 _Complex",
	"__float128",
	"long double",
	"long double _Complex",
	"_Float128x",
	"_Float128x _Complex",
};
// GENERATED END

Type::Type( const Qualifiers &tq, const std::list< Attribute * > & attributes ) : tq( tq ), attributes( attributes ) {}

Type::Type( const Type &other ) : BaseSyntaxNode( other ), tq( other.tq ) {
	cloneAll( other.forall, forall );
	cloneAll( other.attributes, attributes );
}

Type::~Type() {
	deleteAll( forall );
	deleteAll( attributes );
}

// These must remain in the same order as the corresponding bit fields.
const char * Type::FuncSpecifiersNames[] = { "inline", "_Noreturn", "fortran" };
const char * Type::StorageClassesNames[] = { "extern", "static", "auto", "register", "_Thread_local" };
const char * Type::QualifiersNames[] = { "const", "restrict", "volatile", "mutex", "_Atomic" };

Type * Type::stripDeclarator() {
	Type * type, * at;
	for ( type = this; (at = InitTweak::getPointerBase( type )); type = at );
	return type;
}

Type * Type::stripReferences() {
	Type * type;
	ReferenceType * ref;
	for ( type = this; (ref = dynamic_cast<ReferenceType *>( type )); type = ref->base );
	return type;
}

const Type * Type::stripReferences() const {
	const Type * type;
	const ReferenceType * ref;
	for ( type = this; (ref = dynamic_cast<const ReferenceType *>( type )); type = ref->base );
	return type;
}

int Type::referenceDepth() const { return 0; }

TypeSubstitution Type::genericSubstitution() const { assertf( false, "Non-aggregate type: %s", toCString( this ) ); }

void Type::print( std::ostream & os, Indenter indent ) const {
	if ( ! forall.empty() ) {
		os << "forall" << std::endl;
		printAll( forall, os, indent+1 );
		os << ++indent;
	} // if

	if ( ! attributes.empty() ) {
		os << "with attributes" << endl;
		printAll( attributes, os, indent+1 );
	} // if

	tq.print( os );
}


QualifiedType::QualifiedType( const Type::Qualifiers & tq, Type * parent, Type * child ) : Type( tq, {} ), parent( parent ), child( child ) {
}

QualifiedType::QualifiedType( const QualifiedType & other ) : Type( other ), parent( maybeClone( other.parent ) ), child( maybeClone( other.child ) ) {
}

QualifiedType::~QualifiedType() {
	delete parent;
	delete child;
}

void QualifiedType::print( std::ostream & os, Indenter indent ) const {
	os << "Qualified Type:" << endl;
	os << indent+1;
	parent->print( os, indent+1 );
	os << endl << indent+1;
	child->print( os, indent+1 );
	os << endl;
	Type::print( os, indent+1 );
}

GlobalScopeType::GlobalScopeType() : Type( Type::Qualifiers(), {} ) {}

void GlobalScopeType::print( std::ostream & os, Indenter ) const {
	os << "Global Scope Type" << endl;
}


// Empty Variable declarations:
const Type::FuncSpecifiers noFuncSpecifiers;
const Type::StorageClasses noStorageClasses;
const Type::Qualifiers noQualifiers;

bool isUnboundType(const Type * type) {
	if (auto typeInst = dynamic_cast<const TypeInstType *>(type)) {
		// xxx - look for a type name produced by renameTyVars.

		// TODO: once TypeInstType representation is updated, it should properly check
		// if the context id is filled. this is a temporary hack for now
		return isUnboundType(typeInst->name);
	}
	return false;
}

bool isUnboundType(const std::string & tname) {
	// xxx - look for a type name produced by renameTyVars.

	// TODO: once TypeInstType representation is updated, it should properly check
	// if the context id is filled. this is a temporary hack for now
	if (std::count(tname.begin(), tname.end(), '_') >= 3) {
		return true;
	}
	return false;
}

VTableType::VTableType( const Type::Qualifiers &tq, Type *base, const std::list< Attribute * > & attributes )
		: Type( tq, attributes ), base( base ) {
	assertf( base, "VTableType with a null base created." );
}

VTableType::VTableType( const VTableType &other )
		: Type( other ), base( other.base->clone() ) {
}

VTableType::~VTableType() {
	delete base;
}

void VTableType::print( std::ostream &os, Indenter indent ) const {
	Type::print( os, indent );
	os << "get virtual-table type of ";
	if ( base ) {
		base->print( os, indent );
	} // if
}

// Local Variables: //
// tab-width: 4 //
// mode: c++ //
// compile-command: "make install" //
// End: //
