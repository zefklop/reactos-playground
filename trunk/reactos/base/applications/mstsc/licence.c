/* -*- c-basic-offset: 8 -*-
   rdesktop: A Remote Desktop Protocol client.
   RDP licensing negotiation
   Copyright (C) Matthew Chapman 1999-2005
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "todo.h"
//#include <openssl/rc4.h>

void *
ssl_rc4_info_create(void);
void
ssl_rc4_info_delete(void * rc4_info);
void
ssl_rc4_set_key(void * rc4_info, char * key, int len);
void
ssl_rc4_crypt(void * rc4_info, char * in_data, char * out_data, int len);
int
ssl_mod_exp(char* out, int out_len, char* in, int in_len,
            char* mod, int mod_len, char* exp, int exp_len);

extern char g_username[];
extern char g_hostname[];

static uint8 g_licence_key[16];
static uint8 g_licence_sign_key[16];

BOOL g_licence_issued = False;

/* Generate a session key and RC4 keys, given client and server randoms */
static void
licence_generate_keys(uint8 * client_random, uint8 * server_random, uint8 * pre_master_secret)
{
	uint8 master_secret[48];
	uint8 key_block[48];

	/* Generate master secret and then key material */
	sec_hash_48(master_secret, pre_master_secret, client_random, server_random, 'A');
	sec_hash_48(key_block, master_secret, server_random, client_random, 'A');

	/* Store first 16 bytes of session key as MAC secret */
	memcpy(g_licence_sign_key, key_block, 16);

	/* Generate RC4 key from next 16 bytes */
	sec_hash_16(g_licence_key, &key_block[16], client_random, server_random);
}

static void
licence_generate_hwid(uint8 * hwid)
{
	buf_out_uint32(hwid, 2);
	strncpy((char *) (hwid + 4), g_hostname, LICENCE_HWID_SIZE - 4);
}

/* Present an existing licence to the server */
static void
licence_present(uint8 * client_random, uint8 * rsa_data,
		uint8 * licence_data, int licence_size, uint8 * hwid, uint8 * signature)
{
	uint32 sec_flags = SEC_LICENCE_NEG;
	uint16 length =
		16 + SEC_RANDOM_SIZE + SEC_MODULUS_SIZE + SEC_PADDING_SIZE +
		licence_size + LICENCE_HWID_SIZE + LICENCE_SIGNATURE_SIZE;
	STREAM s;

	s = sec_init(sec_flags, length + 4);

	out_uint8(s, LICENCE_TAG_PRESENT);
	out_uint8(s, 2);	/* version */
	out_uint16_le(s, length);

	out_uint32_le(s, 1);
	out_uint16(s, 0);
	out_uint16_le(s, 0x0201);

	out_uint8p(s, client_random, SEC_RANDOM_SIZE);
	out_uint16(s, 0);
	out_uint16_le(s, (SEC_MODULUS_SIZE + SEC_PADDING_SIZE));
	out_uint8p(s, rsa_data, SEC_MODULUS_SIZE);
	out_uint8s(s, SEC_PADDING_SIZE);

	out_uint16_le(s, 1);
	out_uint16_le(s, licence_size);
	out_uint8p(s, licence_data, licence_size);

	out_uint16_le(s, 1);
	out_uint16_le(s, LICENCE_HWID_SIZE);
	out_uint8p(s, hwid, LICENCE_HWID_SIZE);

	out_uint8p(s, signature, LICENCE_SIGNATURE_SIZE);

	s_mark_end(s);
	sec_send(s, sec_flags);
}

/* Send a licence request packet */
static void
licence_send_request(uint8 * client_random, uint8 * rsa_data, char *user, char *host)
{
	uint32 sec_flags = SEC_LICENCE_NEG;
	uint16 userlen = strlen(user) + 1;
	uint16 hostlen = strlen(host) + 1;
	uint16 length = 128 + userlen + hostlen;
	STREAM s;

	s = sec_init(sec_flags, length + 2);

	out_uint8(s, LICENCE_TAG_REQUEST);
	out_uint8(s, 2);	/* version */
	out_uint16_le(s, length);

	out_uint32_le(s, 1);
	out_uint16(s, 0);
	out_uint16_le(s, 0xff01);

	out_uint8p(s, client_random, SEC_RANDOM_SIZE);
	out_uint16(s, 0);
	out_uint16_le(s, (SEC_MODULUS_SIZE + SEC_PADDING_SIZE));
	out_uint8p(s, rsa_data, SEC_MODULUS_SIZE);
	out_uint8s(s, SEC_PADDING_SIZE);

	out_uint16_le(s, LICENCE_TAG_USER);
	out_uint16_le(s, userlen);
	out_uint8p(s, user, userlen);

	out_uint16_le(s, LICENCE_TAG_HOST);
	out_uint16_le(s, hostlen);
	out_uint8p(s, host, hostlen);

	s_mark_end(s);
	sec_send(s, sec_flags);
}

/* Process a licence demand packet */
static void
licence_process_demand(STREAM s)
{
	uint8 null_data[SEC_MODULUS_SIZE];
	uint8 *server_random;
	uint8 signature[LICENCE_SIGNATURE_SIZE];
	uint8 hwid[LICENCE_HWID_SIZE];
	uint8 *licence_data;
	int licence_size;
	void * crypt_key;

	/* Retrieve the server random from the incoming packet */
	in_uint8p(s, server_random, SEC_RANDOM_SIZE);

	/* We currently use null client keys. This is a bit naughty but, hey,
	   the security of licence negotiation isn't exactly paramount. */
	memset(null_data, 0, sizeof(null_data));
	licence_generate_keys(null_data, server_random, null_data);

	licence_size = load_licence(&licence_data);
	if (licence_size > 0)
	{
		/* Generate a signature for the HWID buffer */
		licence_generate_hwid(hwid);
		sec_sign(signature, 16, g_licence_sign_key, 16, hwid, sizeof(hwid));

		/* Now encrypt the HWID */
		crypt_key = ssl_rc4_info_create();
		ssl_rc4_set_key(crypt_key, g_licence_key, 16);
		ssl_rc4_crypt(crypt_key, hwid, hwid, sizeof(hwid));
		ssl_rc4_info_delete(crypt_key);

		licence_present(null_data, null_data, licence_data, licence_size, hwid, signature);
		xfree(licence_data);
		return;
	}

	licence_send_request(null_data, null_data, g_username, g_hostname);
}

/* Send an authentication response packet */
static void
licence_send_authresp(uint8 * token, uint8 * crypt_hwid, uint8 * signature)
{
	uint32 sec_flags = SEC_LICENCE_NEG;
	uint16 length = 58;
	STREAM s;

	s = sec_init(sec_flags, length + 2);

	out_uint8(s, LICENCE_TAG_AUTHRESP);
	out_uint8(s, 2);	/* version */
	out_uint16_le(s, length);

	out_uint16_le(s, 1);
	out_uint16_le(s, LICENCE_TOKEN_SIZE);
	out_uint8p(s, token, LICENCE_TOKEN_SIZE);

	out_uint16_le(s, 1);
	out_uint16_le(s, LICENCE_HWID_SIZE);
	out_uint8p(s, crypt_hwid, LICENCE_HWID_SIZE);

	out_uint8p(s, signature, LICENCE_SIGNATURE_SIZE);

	s_mark_end(s);
	sec_send(s, sec_flags);
}

/* Parse an authentication request packet */
static BOOL
licence_parse_authreq(STREAM s, uint8 ** token, uint8 ** signature)
{
	uint16 tokenlen;

	in_uint8s(s, 6);	/* unknown: f8 3d 15 00 04 f6 */

	in_uint16_le(s, tokenlen);
	if (tokenlen != LICENCE_TOKEN_SIZE)
	{
		error("token len %d\n", tokenlen);
		return False;
	}

	in_uint8p(s, *token, tokenlen);
	in_uint8p(s, *signature, LICENCE_SIGNATURE_SIZE);

	return s_check_end(s);
}

/* Process an authentication request packet */
static void
licence_process_authreq(STREAM s)
{
	uint8 *in_token, *in_sig;
	uint8 out_token[LICENCE_TOKEN_SIZE], decrypt_token[LICENCE_TOKEN_SIZE];
	uint8 hwid[LICENCE_HWID_SIZE], crypt_hwid[LICENCE_HWID_SIZE];
	uint8 sealed_buffer[LICENCE_TOKEN_SIZE + LICENCE_HWID_SIZE];
	uint8 out_sig[LICENCE_SIGNATURE_SIZE];
	void * crypt_key;

	/* Parse incoming packet and save the encrypted token */
	licence_parse_authreq(s, &in_token, &in_sig);
	memcpy(out_token, in_token, LICENCE_TOKEN_SIZE);

	/* Decrypt the token. It should read TEST in Unicode. */
	crypt_key = ssl_rc4_info_create();
	ssl_rc4_set_key(crypt_key, g_licence_key, 16);
	ssl_rc4_crypt(crypt_key, in_token, decrypt_token, LICENCE_TOKEN_SIZE);
	ssl_rc4_info_delete(crypt_key);
	
	/* Generate a signature for a buffer of token and HWID */
	licence_generate_hwid(hwid);
	memcpy(sealed_buffer, decrypt_token, LICENCE_TOKEN_SIZE);
	memcpy(sealed_buffer + LICENCE_TOKEN_SIZE, hwid, LICENCE_HWID_SIZE);
	sec_sign(out_sig, 16, g_licence_sign_key, 16, sealed_buffer, sizeof(sealed_buffer));

	/* Now encrypt the HWID */
	crypt_key = ssl_rc4_info_create();
	ssl_rc4_set_key(crypt_key, g_licence_key, 16);
	ssl_rc4_crypt(crypt_key, hwid, crypt_hwid, LICENCE_HWID_SIZE);
	ssl_rc4_info_delete(crypt_key);

	licence_send_authresp(out_token, crypt_hwid, out_sig);
}

/* Process an licence issue packet */
static void
licence_process_issue(STREAM s)
{
	void * crypt_key;
	uint32 length;
	uint16 check;
	int i;

	in_uint8s(s, 2);	/* 3d 45 - unknown */
	in_uint16_le(s, length);
	if (!s_check_rem(s, length))
		return;

	crypt_key = ssl_rc4_info_create();
	ssl_rc4_set_key(crypt_key, g_licence_key, 16);
	ssl_rc4_crypt(crypt_key, s->p, s->p, length);
	ssl_rc4_info_delete(crypt_key);

	in_uint16(s, check);
	if (check != 0)
		return;

	g_licence_issued = True;

	in_uint8s(s, 2);	/* pad */

	/* advance to fourth string */
	length = 0;
	for (i = 0; i < 4; i++)
	{
		in_uint8s(s, length);
		in_uint32_le(s, length);
		if (!s_check_rem(s, length))
			return;
	}

	g_licence_issued = True;
	save_licence(s->p, length);
}

/* Process a licence packet */
void
licence_process(STREAM s)
{
	uint8 tag;

	in_uint8(s, tag);
	in_uint8s(s, 3);	/* version, length */

	switch (tag)
	{
		case LICENCE_TAG_DEMAND:
			licence_process_demand(s);
			break;

		case LICENCE_TAG_AUTHREQ:
			licence_process_authreq(s);
			break;

		case LICENCE_TAG_ISSUE:
			licence_process_issue(s);
			break;

		case LICENCE_TAG_REISSUE:
		case LICENCE_TAG_RESULT:
			break;

		default:
			unimpl("licence tag 0x%x\n", tag);
	}
}
