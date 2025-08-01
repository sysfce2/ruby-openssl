/*
 * 'OpenSSL for Ruby' project
 * Copyright (C) 2001-2002  Michal Rokos <m.rokos@sh.cvut.cz>
 * All rights reserved.
 */
/*
 * This program is licensed under the same licence as Ruby.
 * (See the file 'COPYING'.)
 */
#include "ossl.h"

#define NewX509(klass) \
    TypedData_Wrap_Struct((klass), &ossl_x509_type, 0)
#define SetX509(obj, x509) do { \
    if (!(x509)) { \
	ossl_raise(rb_eRuntimeError, "CERT wasn't initialized!"); \
    } \
    RTYPEDDATA_DATA(obj) = (x509); \
} while (0)
#define GetX509(obj, x509) do { \
    TypedData_Get_Struct((obj), X509, &ossl_x509_type, (x509)); \
    if (!(x509)) { \
	ossl_raise(rb_eRuntimeError, "CERT wasn't initialized!"); \
    } \
} while (0)

/*
 * Classes
 */
VALUE cX509Cert;
static VALUE eX509CertError;

static void
ossl_x509_free(void *ptr)
{
    X509_free(ptr);
}

static const rb_data_type_t ossl_x509_type = {
    "OpenSSL/X509",
    {
	0, ossl_x509_free,
    },
    0, 0, RUBY_TYPED_FREE_IMMEDIATELY | RUBY_TYPED_WB_PROTECTED,
};

/*
 * Public
 */
VALUE
ossl_x509_new(X509 *x509)
{
    X509 *new;
    VALUE obj;

    obj = NewX509(cX509Cert);
    new = X509_dup(x509);
    if (!new)
        ossl_raise(eX509CertError, "X509_dup");
    SetX509(obj, new);

    return obj;
}

X509 *
GetX509CertPtr(VALUE obj)
{
    X509 *x509;

    GetX509(obj, x509);

    return x509;
}

X509 *
DupX509CertPtr(VALUE obj)
{
    X509 *x509;

    GetX509(obj, x509);

    X509_up_ref(x509);

    return x509;
}

/*
 * Private
 */
static VALUE
ossl_x509_alloc(VALUE klass)
{
    X509 *x509;
    VALUE obj;

    obj = NewX509(klass);
    x509 = X509_new();
    if (!x509) ossl_raise(eX509CertError, NULL);
    SetX509(obj, x509);

    return obj;
}

/*
 * call-seq:
 *    Certificate.new => cert
 *    Certificate.new(string) => cert
 */
static VALUE
ossl_x509_initialize(int argc, VALUE *argv, VALUE self)
{
    BIO *in;
    X509 *x509, *x509_orig = RTYPEDDATA_DATA(self);
    VALUE arg;

    rb_check_frozen(self);
    if (rb_scan_args(argc, argv, "01", &arg) == 0) {
	/* create just empty X509Cert */
	return self;
    }
    arg = ossl_to_der_if_possible(arg);
    in = ossl_obj2bio(&arg);
    x509 = d2i_X509_bio(in, NULL);
    if (!x509) {
        OSSL_BIO_reset(in);
        x509 = PEM_read_bio_X509(in, NULL, NULL, NULL);
    }
    BIO_free(in);
    if (!x509)
        ossl_raise(eX509CertError, "PEM_read_bio_X509");

    RTYPEDDATA_DATA(self) = x509;
    X509_free(x509_orig);

    return self;
}

/* :nodoc: */
static VALUE
ossl_x509_copy(VALUE self, VALUE other)
{
    X509 *a, *b, *x509;

    rb_check_frozen(self);
    if (self == other) return self;

    GetX509(self, a);
    GetX509(other, b);

    x509 = X509_dup(b);
    if (!x509) ossl_raise(eX509CertError, NULL);

    DATA_PTR(self) = x509;
    X509_free(a);

    return self;
}

/*
 * call-seq:
 *    cert.to_der => string
 */
static VALUE
ossl_x509_to_der(VALUE self)
{
    X509 *x509;
    VALUE str;
    long len;
    unsigned char *p;

    GetX509(self, x509);
    if ((len = i2d_X509(x509, NULL)) <= 0)
	ossl_raise(eX509CertError, NULL);
    str = rb_str_new(0, len);
    p = (unsigned char *)RSTRING_PTR(str);
    if (i2d_X509(x509, &p) <= 0)
	ossl_raise(eX509CertError, NULL);
    ossl_str_adjust(str, p);

    return str;
}

/*
 * call-seq:
 *    cert.to_pem => string
 */
static VALUE
ossl_x509_to_pem(VALUE self)
{
    X509 *x509;
    BIO *out;
    VALUE str;

    GetX509(self, x509);
    out = BIO_new(BIO_s_mem());
    if (!out) ossl_raise(eX509CertError, NULL);

    if (!PEM_write_bio_X509(out, x509)) {
	BIO_free(out);
	ossl_raise(eX509CertError, NULL);
    }
    str = ossl_membio2str(out);

    return str;
}

/*
 * call-seq:
 *    cert.to_text => string
 */
static VALUE
ossl_x509_to_text(VALUE self)
{
    X509 *x509;
    BIO *out;
    VALUE str;

    GetX509(self, x509);

    out = BIO_new(BIO_s_mem());
    if (!out) ossl_raise(eX509CertError, NULL);

    if (!X509_print(out, x509)) {
	BIO_free(out);
	ossl_raise(eX509CertError, NULL);
    }
    str = ossl_membio2str(out);

    return str;
}

#if 0
/*
 * Makes from X509 X509_REQuest
 */
static VALUE
ossl_x509_to_req(VALUE self)
{
    X509 *x509;
    X509_REQ *req;
    VALUE obj;

    GetX509(self, x509);
    if (!(req = X509_to_X509_REQ(x509, NULL, EVP_md5()))) {
	ossl_raise(eX509CertError, NULL);
    }
    obj = ossl_x509req_new(req);
    X509_REQ_free(req);

    return obj;
}
#endif

/*
 * call-seq:
 *    cert.version => integer
 */
static VALUE
ossl_x509_get_version(VALUE self)
{
    X509 *x509;

    GetX509(self, x509);

    return LONG2NUM(X509_get_version(x509));
}

/*
 * call-seq:
 *    cert.version = integer => integer
 */
static VALUE
ossl_x509_set_version(VALUE self, VALUE version)
{
    X509 *x509;
    long ver;

    if ((ver = NUM2LONG(version)) < 0) {
	ossl_raise(eX509CertError, "version must be >= 0!");
    }
    GetX509(self, x509);
    if (!X509_set_version(x509, ver)) {
	ossl_raise(eX509CertError, NULL);
    }

    return version;
}

/*
 * call-seq:
 *    cert.serial => integer
 */
static VALUE
ossl_x509_get_serial(VALUE self)
{
    X509 *x509;

    GetX509(self, x509);

    return asn1integer_to_num(X509_get_serialNumber(x509));
}

/*
 * call-seq:
 *    cert.serial = integer => integer
 */
static VALUE
ossl_x509_set_serial(VALUE self, VALUE num)
{
    X509 *x509;

    GetX509(self, x509);
    X509_set_serialNumber(x509, num_to_asn1integer(num, X509_get_serialNumber(x509)));

    return num;
}

/*
 * call-seq:
 *    cert.signature_algorithm => string
 */
static VALUE
ossl_x509_get_signature_algorithm(VALUE self)
{
    X509 *x509;
    BIO *out;
    const ASN1_OBJECT *obj;
    VALUE str;

    GetX509(self, x509);
    out = BIO_new(BIO_s_mem());
    if (!out) ossl_raise(eX509CertError, NULL);

    X509_ALGOR_get0(&obj, NULL, NULL, X509_get0_tbs_sigalg(x509));
    if (!i2a_ASN1_OBJECT(out, obj)) {
	BIO_free(out);
	ossl_raise(eX509CertError, NULL);
    }
    str = ossl_membio2str(out);

    return str;
}

/*
 * call-seq:
 *    cert.subject => name
 */
static VALUE
ossl_x509_get_subject(VALUE self)
{
    X509 *x509;
    X509_NAME *name;

    GetX509(self, x509);
    if (!(name = X509_get_subject_name(x509))) { /* NO DUP - don't free! */
	ossl_raise(eX509CertError, NULL);
    }

    return ossl_x509name_new(name);
}

/*
 * call-seq:
 *    cert.subject = name => name
 */
static VALUE
ossl_x509_set_subject(VALUE self, VALUE subject)
{
    X509 *x509;

    GetX509(self, x509);
    if (!X509_set_subject_name(x509, GetX509NamePtr(subject))) { /* DUPs name */
	ossl_raise(eX509CertError, NULL);
    }

    return subject;
}

/*
 * call-seq:
 *    cert.issuer => name
 */
static VALUE
ossl_x509_get_issuer(VALUE self)
{
    X509 *x509;
    X509_NAME *name;

    GetX509(self, x509);
    if(!(name = X509_get_issuer_name(x509))) { /* NO DUP - don't free! */
	ossl_raise(eX509CertError, NULL);
    }

    return ossl_x509name_new(name);
}

/*
 * call-seq:
 *    cert.issuer = name => name
 */
static VALUE
ossl_x509_set_issuer(VALUE self, VALUE issuer)
{
    X509 *x509;

    GetX509(self, x509);
    if (!X509_set_issuer_name(x509, GetX509NamePtr(issuer))) { /* DUPs name */
	ossl_raise(eX509CertError, NULL);
    }

    return issuer;
}

/*
 * call-seq:
 *    cert.not_before => time
 */
static VALUE
ossl_x509_get_not_before(VALUE self)
{
    X509 *x509;
    const ASN1_TIME *asn1time;

    GetX509(self, x509);
    if (!(asn1time = X509_get0_notBefore(x509))) {
	ossl_raise(eX509CertError, NULL);
    }

    return asn1time_to_time(asn1time);
}

/*
 * call-seq:
 *    cert.not_before = time => time
 */
static VALUE
ossl_x509_set_not_before(VALUE self, VALUE time)
{
    X509 *x509;
    ASN1_TIME *asn1time;

    GetX509(self, x509);
    asn1time = ossl_x509_time_adjust(NULL, time);
    if (!X509_set1_notBefore(x509, asn1time)) {
	ASN1_TIME_free(asn1time);
	ossl_raise(eX509CertError, "X509_set_notBefore");
    }
    ASN1_TIME_free(asn1time);

    return time;
}

/*
 * call-seq:
 *    cert.not_after => time
 */
static VALUE
ossl_x509_get_not_after(VALUE self)
{
    X509 *x509;
    const ASN1_TIME *asn1time;

    GetX509(self, x509);
    if (!(asn1time = X509_get0_notAfter(x509))) {
	ossl_raise(eX509CertError, NULL);
    }

    return asn1time_to_time(asn1time);
}

/*
 * call-seq:
 *    cert.not_after = time => time
 */
static VALUE
ossl_x509_set_not_after(VALUE self, VALUE time)
{
    X509 *x509;
    ASN1_TIME *asn1time;

    GetX509(self, x509);
    asn1time = ossl_x509_time_adjust(NULL, time);
    if (!X509_set1_notAfter(x509, asn1time)) {
	ASN1_TIME_free(asn1time);
	ossl_raise(eX509CertError, "X509_set_notAfter");
    }
    ASN1_TIME_free(asn1time);

    return time;
}

/*
 * call-seq:
 *    cert.public_key => key
 */
static VALUE
ossl_x509_get_public_key(VALUE self)
{
    X509 *x509;
    EVP_PKEY *pkey;

    GetX509(self, x509);
    if (!(pkey = X509_get_pubkey(x509))) { /* adds an reference */
	ossl_raise(eX509CertError, NULL);
    }

    return ossl_pkey_wrap(pkey);
}

/*
 * call-seq:
 *    cert.public_key = key
 */
static VALUE
ossl_x509_set_public_key(VALUE self, VALUE key)
{
    X509 *x509;
    EVP_PKEY *pkey;

    GetX509(self, x509);
    pkey = GetPKeyPtr(key);
    ossl_pkey_check_public_key(pkey);
    if (!X509_set_pubkey(x509, pkey))
	ossl_raise(eX509CertError, "X509_set_pubkey");
    return key;
}

/*
 * call-seq:
 *    cert.sign(key, digest) => self
 */
static VALUE
ossl_x509_sign(VALUE self, VALUE key, VALUE digest)
{
    X509 *x509;
    EVP_PKEY *pkey;
    const EVP_MD *md;

    pkey = GetPrivPKeyPtr(key); /* NO NEED TO DUP */
    if (NIL_P(digest)) {
        md = NULL; /* needed for some key types, e.g. Ed25519 */
    } else {
        md = ossl_evp_get_digestbyname(digest);
    }
    GetX509(self, x509);
    if (!X509_sign(x509, pkey, md)) {
	ossl_raise(eX509CertError, NULL);
    }

    return self;
}

/*
 * call-seq:
 *    cert.verify(key) => true | false
 *
 * Verifies the signature of the certificate, with the public key _key_. _key_
 * must be an instance of OpenSSL::PKey.
 */
static VALUE
ossl_x509_verify(VALUE self, VALUE key)
{
    X509 *x509;
    EVP_PKEY *pkey;

    GetX509(self, x509);
    pkey = GetPKeyPtr(key);
    ossl_pkey_check_public_key(pkey);
    switch (X509_verify(x509, pkey)) {
      case 1:
	return Qtrue;
      case 0:
	ossl_clear_error();
	return Qfalse;
      default:
	ossl_raise(eX509CertError, NULL);
    }
}

/*
 * call-seq:
 *    cert.check_private_key(key) -> true | false
 *
 * Returns +true+ if _key_ is the corresponding private key to the Subject
 * Public Key Information, +false+ otherwise.
 */
static VALUE
ossl_x509_check_private_key(VALUE self, VALUE key)
{
    X509 *x509;
    EVP_PKEY *pkey;

    /* not needed private key, but should be */
    pkey = GetPrivPKeyPtr(key); /* NO NEED TO DUP */
    GetX509(self, x509);
    if (!X509_check_private_key(x509, pkey)) {
	ossl_clear_error();
	return Qfalse;
    }

    return Qtrue;
}

/*
 * call-seq:
 *    cert.extensions => [extension...]
 */
static VALUE
ossl_x509_get_extensions(VALUE self)
{
    X509 *x509;
    int count, i;
    X509_EXTENSION *ext;
    VALUE ary;

    GetX509(self, x509);
    count = X509_get_ext_count(x509);
    ary = rb_ary_new_capa(count);
    for (i=0; i<count; i++) {
	ext = X509_get_ext(x509, i); /* NO DUP - don't free! */
	rb_ary_push(ary, ossl_x509ext_new(ext));
    }

    return ary;
}

/*
 * call-seq:
 *    cert.extensions = [ext...] => [ext...]
 */
static VALUE
ossl_x509_set_extensions(VALUE self, VALUE ary)
{
    X509 *x509;
    X509_EXTENSION *ext;
    long i;

    Check_Type(ary, T_ARRAY);
    /* All ary's members should be X509Extension */
    for (i=0; i<RARRAY_LEN(ary); i++) {
	OSSL_Check_Kind(RARRAY_AREF(ary, i), cX509Ext);
    }
    GetX509(self, x509);
    for (i = X509_get_ext_count(x509); i > 0; i--)
        X509_EXTENSION_free(X509_delete_ext(x509, 0));
    for (i=0; i<RARRAY_LEN(ary); i++) {
	ext = GetX509ExtPtr(RARRAY_AREF(ary, i));
	if (!X509_add_ext(x509, ext, -1)) { /* DUPs ext */
	    ossl_raise(eX509CertError, "X509_add_ext");
	}
    }

    return ary;
}

/*
 * call-seq:
 *    cert.add_extension(extension) => extension
 */
static VALUE
ossl_x509_add_extension(VALUE self, VALUE extension)
{
    X509 *x509;
    X509_EXTENSION *ext;

    GetX509(self, x509);
    ext = GetX509ExtPtr(extension);
    if (!X509_add_ext(x509, ext, -1)) { /* DUPs ext - FREE it */
	ossl_raise(eX509CertError, NULL);
    }

    return extension;
}

static VALUE
ossl_x509_inspect(VALUE self)
{
    return rb_sprintf("#<%"PRIsVALUE": subject=%+"PRIsVALUE", "
		      "issuer=%+"PRIsVALUE", serial=%+"PRIsVALUE", "
		      "not_before=%+"PRIsVALUE", not_after=%+"PRIsVALUE">",
		      rb_obj_class(self),
		      ossl_x509_get_subject(self),
		      ossl_x509_get_issuer(self),
		      ossl_x509_get_serial(self),
		      ossl_x509_get_not_before(self),
		      ossl_x509_get_not_after(self));
}

/*
 * call-seq:
 *    cert1 == cert2 -> true | false
 *
 * Compares the two certificates. Note that this takes into account all fields,
 * not just the issuer name and the serial number.
 */
static VALUE
ossl_x509_eq(VALUE self, VALUE other)
{
    X509 *a, *b;

    GetX509(self, a);
    if (!rb_obj_is_kind_of(other, cX509Cert))
	return Qfalse;
    GetX509(other, b);

    return !X509_cmp(a, b) ? Qtrue : Qfalse;
}

/*
 * call-seq:
 *    cert.tbs_bytes => string
 *
 * Returns the DER-encoded bytes of the certificate's to be signed certificate.
 * This is mainly useful for validating embedded certificate transparency signatures.
 */
static VALUE
ossl_x509_tbs_bytes(VALUE self)
{
    X509 *x509;
    int len;
    unsigned char *p0;
    VALUE str;

    GetX509(self, x509);
    len = i2d_re_X509_tbs(x509, NULL);
    if (len <= 0) {
        ossl_raise(eX509CertError, "i2d_re_X509_tbs");
    }
    str = rb_str_new(NULL, len);
    p0 = (unsigned char *)RSTRING_PTR(str);
    if (i2d_re_X509_tbs(x509, &p0) <= 0) {
        ossl_raise(eX509CertError, "i2d_re_X509_tbs");
    }
    ossl_str_adjust(str, p0);

    return str;
}

struct load_chained_certificates_arguments {
    VALUE certificates;
    X509 *certificate;
};

static VALUE
load_chained_certificates_append_push(VALUE _arguments) {
    struct load_chained_certificates_arguments *arguments = (struct load_chained_certificates_arguments*)_arguments;

    if (arguments->certificates == Qnil) {
        arguments->certificates = rb_ary_new();
    }

    rb_ary_push(arguments->certificates, ossl_x509_new(arguments->certificate));

    return Qnil;
}

static VALUE
load_chained_certificate_append_ensure(VALUE _arguments) {
    struct load_chained_certificates_arguments *arguments = (struct load_chained_certificates_arguments*)_arguments;

    X509_free(arguments->certificate);

    return Qnil;
}

inline static VALUE
load_chained_certificates_append(VALUE certificates, X509 *certificate) {
    struct load_chained_certificates_arguments arguments;
    arguments.certificates = certificates;
    arguments.certificate = certificate;

    rb_ensure(load_chained_certificates_append_push, (VALUE)&arguments, load_chained_certificate_append_ensure, (VALUE)&arguments);

    return arguments.certificates;
}

static VALUE
load_chained_certificates_PEM(BIO *in) {
    VALUE certificates = Qnil;
    X509 *certificate = PEM_read_bio_X509(in, NULL, NULL, NULL);

    /* If we cannot read even one certificate: */
    if (certificate == NULL) {
        /* If we cannot read one certificate because we could not read the PEM encoding: */
        if (ERR_GET_REASON(ERR_peek_last_error()) == PEM_R_NO_START_LINE) {
            ossl_clear_error();
        }

        if (ERR_peek_last_error())
            ossl_raise(eX509CertError, NULL);
        else
            return Qnil;
    }

    certificates = load_chained_certificates_append(Qnil, certificate);

    while ((certificate = PEM_read_bio_X509(in, NULL, NULL, NULL))) {
      load_chained_certificates_append(certificates, certificate);
    }

    /* We tried to read one more certificate but could not read start line: */
    if (ERR_GET_REASON(ERR_peek_last_error()) == PEM_R_NO_START_LINE) {
        /* This is not an error, it means we are finished: */
        ossl_clear_error();

        return certificates;
    }

    /* Alternatively, if we reached the end of the file and there was no error: */
    if (BIO_eof(in) && !ERR_peek_last_error()) {
        return certificates;
    } else {
        /* Otherwise, we tried to read a certificate but failed somewhere: */
        ossl_raise(eX509CertError, NULL);
    }
}

static VALUE
load_chained_certificates_DER(BIO *in) {
    X509 *certificate = d2i_X509_bio(in, NULL);

    /* If we cannot read one certificate: */
    if (certificate == NULL) {
        /* Ignore error. We could not load. */
        ossl_clear_error();

        return Qnil;
    }

    return load_chained_certificates_append(Qnil, certificate);
}

static VALUE
load_chained_certificates(VALUE _io) {
    BIO *in = (BIO*)_io;
    VALUE certificates = Qnil;

    /*
      DER is a binary format and it may contain octets within it that look like
      PEM encoded certificates. So we need to check DER first.
    */
    certificates = load_chained_certificates_DER(in);

    if (certificates != Qnil)
        return certificates;

    OSSL_BIO_reset(in);

    certificates = load_chained_certificates_PEM(in);

    if (certificates != Qnil)
        return certificates;

    /* Otherwise we couldn't read the output correctly so fail: */
    ossl_raise(eX509CertError, "Could not detect format of certificate data!");
}

static VALUE
load_chained_certificates_ensure(VALUE _io) {
    BIO *in = (BIO*)_io;

    BIO_free(in);

    return Qnil;
}

/*
 * call-seq:
 *    OpenSSL::X509::Certificate.load(string) -> [certs...]
 *    OpenSSL::X509::Certificate.load(file) -> [certs...]
 *
 * Read the chained certificates from the given input. Supports both PEM
 * and DER encoded certificates.
 *
 * PEM is a text format and supports more than one certificate.
 *
 * DER is a binary format and only supports one certificate.
 *
 * If the file is empty, or contains only unrelated data, an
 * +OpenSSL::X509::CertificateError+ exception will be raised.
 */
static VALUE
ossl_x509_load(VALUE klass, VALUE buffer)
{
    BIO *in = ossl_obj2bio(&buffer);

    return rb_ensure(load_chained_certificates, (VALUE)in, load_chained_certificates_ensure, (VALUE)in);
}

/*
 * INIT
 */
void
Init_ossl_x509cert(void)
{
#if 0
    mOSSL = rb_define_module("OpenSSL");
    eOSSLError = rb_define_class_under(mOSSL, "OpenSSLError", rb_eStandardError);
    mX509 = rb_define_module_under(mOSSL, "X509");
#endif

    eX509CertError = rb_define_class_under(mX509, "CertificateError", eOSSLError);

    /* Document-class: OpenSSL::X509::Certificate
     *
     * Implementation of an X.509 certificate as specified in RFC 5280.
     * Provides access to a certificate's attributes and allows certificates
     * to be read from a string, but also supports the creation of new
     * certificates from scratch.
     *
     * === Reading a certificate from a file
     *
     * Certificate is capable of handling DER-encoded certificates and
     * certificates encoded in OpenSSL's PEM format.
     *
     *   raw = File.binread "cert.cer" # DER- or PEM-encoded
     *   certificate = OpenSSL::X509::Certificate.new raw
     *
     * === Saving a certificate to a file
     *
     * A certificate may be encoded in DER format
     *
     *   cert = ...
     *   File.open("cert.cer", "wb") { |f| f.print cert.to_der }
     *
     * or in PEM format
     *
     *   cert = ...
     *   File.open("cert.pem", "wb") { |f| f.print cert.to_pem }
     *
     * X.509 certificates are associated with a private/public key pair,
     * typically a RSA, DSA or ECC key (see also OpenSSL::PKey::RSA,
     * OpenSSL::PKey::DSA and OpenSSL::PKey::EC), the public key itself is
     * stored within the certificate and can be accessed in form of an
     * OpenSSL::PKey. Certificates are typically used to be able to associate
     * some form of identity with a key pair, for example web servers serving
     * pages over HTTPs use certificates to authenticate themselves to the user.
     *
     * The public key infrastructure (PKI) model relies on trusted certificate
     * authorities ("root CAs") that issue these certificates, so that end
     * users need to base their trust just on a selected few authorities
     * that themselves again vouch for subordinate CAs issuing their
     * certificates to end users.
     *
     * The OpenSSL::X509 module provides the tools to set up an independent
     * PKI, similar to scenarios where the 'openssl' command line tool is
     * used for issuing certificates in a private PKI.
     *
     * === Creating a root CA certificate and an end-entity certificate
     *
     * First, we need to create a "self-signed" root certificate. To do so,
     * we need to generate a key first. Please note that the choice of "1"
     * as a serial number is considered a security flaw for real certificates.
     * Secure choices are integers in the two-digit byte range and ideally
     * not sequential but secure random numbers, steps omitted here to keep
     * the example concise.
     *
     *   root_key = OpenSSL::PKey::RSA.new 2048 # the CA's public/private key
     *   root_ca = OpenSSL::X509::Certificate.new
     *   root_ca.version = 2 # cf. RFC 5280 - to make it a "v3" certificate
     *   root_ca.serial = 1
     *   root_ca.subject = OpenSSL::X509::Name.parse "/DC=org/DC=ruby-lang/CN=Ruby CA"
     *   root_ca.issuer = root_ca.subject # root CA's are "self-signed"
     *   root_ca.public_key = root_key.public_key
     *   root_ca.not_before = Time.now
     *   root_ca.not_after = root_ca.not_before + 2 * 365 * 24 * 60 * 60 # 2 years validity
     *   ef = OpenSSL::X509::ExtensionFactory.new
     *   ef.subject_certificate = root_ca
     *   ef.issuer_certificate = root_ca
     *   root_ca.add_extension(ef.create_extension("basicConstraints","CA:TRUE",true))
     *   root_ca.add_extension(ef.create_extension("keyUsage","keyCertSign, cRLSign", true))
     *   root_ca.add_extension(ef.create_extension("subjectKeyIdentifier","hash",false))
     *   root_ca.add_extension(ef.create_extension("authorityKeyIdentifier","keyid:always",false))
     *   root_ca.sign(root_key, OpenSSL::Digest.new('SHA256'))
     *
     * The next step is to create the end-entity certificate using the root CA
     * certificate.
     *
     *   key = OpenSSL::PKey::RSA.new 2048
     *   cert = OpenSSL::X509::Certificate.new
     *   cert.version = 2
     *   cert.serial = 2
     *   cert.subject = OpenSSL::X509::Name.parse "/DC=org/DC=ruby-lang/CN=Ruby certificate"
     *   cert.issuer = root_ca.subject # root CA is the issuer
     *   cert.public_key = key.public_key
     *   cert.not_before = Time.now
     *   cert.not_after = cert.not_before + 1 * 365 * 24 * 60 * 60 # 1 years validity
     *   ef = OpenSSL::X509::ExtensionFactory.new
     *   ef.subject_certificate = cert
     *   ef.issuer_certificate = root_ca
     *   cert.add_extension(ef.create_extension("keyUsage","digitalSignature", true))
     *   cert.add_extension(ef.create_extension("subjectKeyIdentifier","hash",false))
     *   cert.sign(root_key, OpenSSL::Digest.new('SHA256'))
     *
     */
    cX509Cert = rb_define_class_under(mX509, "Certificate", rb_cObject);

    rb_define_singleton_method(cX509Cert, "load", ossl_x509_load, 1);

    rb_define_alloc_func(cX509Cert, ossl_x509_alloc);
    rb_define_method(cX509Cert, "initialize", ossl_x509_initialize, -1);
    rb_define_method(cX509Cert, "initialize_copy", ossl_x509_copy, 1);

    rb_define_method(cX509Cert, "to_der", ossl_x509_to_der, 0);
    rb_define_method(cX509Cert, "to_pem", ossl_x509_to_pem, 0);
    rb_define_alias(cX509Cert, "to_s", "to_pem");
    rb_define_method(cX509Cert, "to_text", ossl_x509_to_text, 0);
    rb_define_method(cX509Cert, "version", ossl_x509_get_version, 0);
    rb_define_method(cX509Cert, "version=", ossl_x509_set_version, 1);
    rb_define_method(cX509Cert, "signature_algorithm", ossl_x509_get_signature_algorithm, 0);
    rb_define_method(cX509Cert, "serial", ossl_x509_get_serial, 0);
    rb_define_method(cX509Cert, "serial=", ossl_x509_set_serial, 1);
    rb_define_method(cX509Cert, "subject", ossl_x509_get_subject, 0);
    rb_define_method(cX509Cert, "subject=", ossl_x509_set_subject, 1);
    rb_define_method(cX509Cert, "issuer", ossl_x509_get_issuer, 0);
    rb_define_method(cX509Cert, "issuer=", ossl_x509_set_issuer, 1);
    rb_define_method(cX509Cert, "not_before", ossl_x509_get_not_before, 0);
    rb_define_method(cX509Cert, "not_before=", ossl_x509_set_not_before, 1);
    rb_define_method(cX509Cert, "not_after", ossl_x509_get_not_after, 0);
    rb_define_method(cX509Cert, "not_after=", ossl_x509_set_not_after, 1);
    rb_define_method(cX509Cert, "public_key", ossl_x509_get_public_key, 0);
    rb_define_method(cX509Cert, "public_key=", ossl_x509_set_public_key, 1);
    rb_define_method(cX509Cert, "sign", ossl_x509_sign, 2);
    rb_define_method(cX509Cert, "verify", ossl_x509_verify, 1);
    rb_define_method(cX509Cert, "check_private_key", ossl_x509_check_private_key, 1);
    rb_define_method(cX509Cert, "extensions", ossl_x509_get_extensions, 0);
    rb_define_method(cX509Cert, "extensions=", ossl_x509_set_extensions, 1);
    rb_define_method(cX509Cert, "add_extension", ossl_x509_add_extension, 1);
    rb_define_method(cX509Cert, "inspect", ossl_x509_inspect, 0);
    rb_define_method(cX509Cert, "==", ossl_x509_eq, 1);
    rb_define_method(cX509Cert, "tbs_bytes", ossl_x509_tbs_bytes, 0);
}
