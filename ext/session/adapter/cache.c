
/*
  +------------------------------------------------------------------------+
  | Phalcon Framework                                                      |
  +------------------------------------------------------------------------+
  | Copyright (c) 2011-2014 Phalcon Team (http://www.phalconphp.com)       |
  +------------------------------------------------------------------------+
  | This source file is subject to the New BSD License that is bundled     |
  | with this package in the file docs/LICENSE.txt.                        |
  |                                                                        |
  | If you did not receive a copy of the license and are unable to         |
  | obtain it through the world-wide-web, please send an email             |
  | to license@phalconphp.com so we can send you a copy immediately.       |
  +------------------------------------------------------------------------+
  | Authors: Andres Gutierrez <andres@phalconphp.com>                      |
  |          Eduar Carvajal <eduar@phalconphp.com>                         |
  |          ZhuZongXin <dreamsxin@qq.com>                                 |
  +------------------------------------------------------------------------+
*/

#include "session/adapter/cache.h"
#include "session/adapter.h"
#include "session/adapterinterface.h"
#include "session/exception.h"
#include "cache/backendinterface.h"

#ifdef PHALCON_USE_PHP_SESSION
#include <ext/session/php_session.h>
#endif

#include "kernel/main.h"
#include "kernel/exception.h"
#include "kernel/fcall.h"
#include "kernel/string.h"
#include "kernel/concat.h"
#include "kernel/array.h"
#include "kernel/object.h"

/**
 * Phalcon\Session\Adapter\Cache
 *
 * This adapter store sessions in cache
 *
 *<code>
 * $session = new Phalcon\Session\Adapter\Cache(array(
 *     'servers' => array(
 *         array('host' => 'localhost', 'port' => 11211, 'weight' => 1),
 *     ),
 *     'client' => array(
 *         Memcached::OPT_HASH => Memcached::HASH_MD5,
 *         Memcached::OPT_PREFIX_KEY => 'prefix.',
 *     ),
 *    'lifetime' => 3600,
 *    'prefix' => 'my_'
 * ));
 *
 * $session->start();
 *
 * $session->set('var', 'some-value');
 *
 * echo $session->get('var');
 *</code>
 */
zend_class_entry *phalcon_session_adapter_cache_ce;

PHP_METHOD(Phalcon_Session_Adapter_Cache, __construct);
PHP_METHOD(Phalcon_Session_Adapter_Cache, open);
PHP_METHOD(Phalcon_Session_Adapter_Cache, close);
PHP_METHOD(Phalcon_Session_Adapter_Cache, read);
PHP_METHOD(Phalcon_Session_Adapter_Cache, write);
PHP_METHOD(Phalcon_Session_Adapter_Cache, destroy);
PHP_METHOD(Phalcon_Session_Adapter_Cache, gc);

ZEND_BEGIN_ARG_INFO_EX(arginfo_phalcon_session_adapter_cache___construct, 0, 0, 1)
	ZEND_ARG_INFO(0, options)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_phalcon_session_adapter_cache_read, 0, 0, 1)
	ZEND_ARG_INFO(0, sessionId)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_phalcon_session_adapter_cache_write, 0, 0, 2)
	ZEND_ARG_INFO(0, sessionId)
	ZEND_ARG_INFO(0, data)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_phalcon_session_adapter_cache_destroy, 0, 0, 0)
        ZEND_ARG_INFO(0, sessionId)
ZEND_END_ARG_INFO()

static const zend_function_entry phalcon_session_adapter_cache_method_entry[] = {
	PHP_ME(Phalcon_Session_Adapter_Cache, __construct, arginfo_phalcon_session_adapter_cache___construct, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
	PHP_ME(Phalcon_Session_Adapter_Cache, open, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(Phalcon_Session_Adapter_Cache, close, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(Phalcon_Session_Adapter_Cache, read, arginfo_phalcon_session_adapter_cache_read, ZEND_ACC_PUBLIC)
	PHP_ME(Phalcon_Session_Adapter_Cache, write, arginfo_phalcon_session_adapter_cache_write, ZEND_ACC_PUBLIC)
	PHP_ME(Phalcon_Session_Adapter_Cache, destroy, arginfo_phalcon_session_adapter_cache_destroy, ZEND_ACC_PUBLIC)
	PHP_ME(Phalcon_Session_Adapter_Cache, gc, NULL, ZEND_ACC_PUBLIC)
	PHP_FE_END
};

/**
 * Phalcon\Session\Adapter\Cache initializer
 */
PHALCON_INIT_CLASS(Phalcon_Session_Adapter_Cache){

	PHALCON_REGISTER_CLASS_EX(Phalcon\\Session\\Adapter, Cache, session_adapter_cache, phalcon_session_adapter_ce, phalcon_session_adapter_cache_method_entry, 0);

	zend_declare_property_long(phalcon_session_adapter_cache_ce, SL("_lifetime"), 8600, ZEND_ACC_PROTECTED);
	zend_declare_property_null(phalcon_session_adapter_cache_ce, SL("_cache"), ZEND_ACC_PROTECTED);

	zend_class_implements(phalcon_session_adapter_cache_ce, 1, phalcon_session_adapterinterface_ce);

	return SUCCESS;
}

/**
 * Constructor for Phalcon\Session\Adapter\Cache
 *
 * @param array $options
 */
PHP_METHOD(Phalcon_Session_Adapter_Cache, __construct){

	zval *options, service = {}, cache = {}, lifetime = {};
	zval callable_open = {}, callable_close = {}, callable_read  = {}, callable_write = {}, callable_destroy = {}, callable_gc = {};

	phalcon_fetch_params(0, 1, 0, &options);

	if (Z_TYPE_P(options) != IS_ARRAY) { 
		PHALCON_THROW_EXCEPTION_STRW(phalcon_session_exception_ce, "The options must be an array");
		return;
	}

	if (!phalcon_array_isset_fetch_str(&service, options, SL("service"))) {
		PHALCON_THROW_EXCEPTION_STRW(phalcon_session_exception_ce, "No service given in options");
		return;
	}

	if (Z_TYPE(service) != IS_OBJECT) {
		PHALCON_CALL_METHODW(&cache, getThis(), "getresolveservice", &service);
	} else {
		PHALCON_CPY_WRT(&cache, &service);
	}
	PHALCON_VERIFY_INTERFACEW(&cache, phalcon_cache_backendinterface_ce);

	phalcon_update_property_zval(getThis(), SL("_cache"), &cache);

	if (phalcon_array_isset_fetch_str(&lifetime, options, SL("lifetime"))) {
		phalcon_update_property_zval(getThis(), SL("_lifetime"), &lifetime);
	}

	phalcon_update_property_zval(getThis(), SL("_cache"), &cache);

	/* open callback */
	array_init_size(&callable_open, 2);
	phalcon_array_append(&callable_open, getThis(), 0);
	phalcon_array_append_string(&callable_open, SL("open"), 0);

	/* close callback */
	array_init_size(&callable_close, 2);
	phalcon_array_append(&callable_close, getThis(), 0);
	phalcon_array_append_string(&callable_close, SL("close"), 0);

	/* read callback */
	array_init_size(&callable_read, 2);
	phalcon_array_append(&callable_read, getThis(), 0);
	phalcon_array_append_string(&callable_read, SL("read"), 0);

	/* write callback */
	array_init_size(&callable_write, 2);
	phalcon_array_append(&callable_write, getThis(), 0);
	phalcon_array_append_string(&callable_write, SL("write"), 0);

	/* destroy callback */
	array_init_size(&callable_destroy, 2);
	phalcon_array_append(&callable_destroy, getThis(), 0);
	phalcon_array_append_string(&callable_destroy, SL("destroy"), 0);

	/* gc callback */
	array_init_size(&callable_gc, 2);
	phalcon_array_append(&callable_gc, getThis(), 0);
	phalcon_array_append_string(&callable_gc, SL("gc"), 0);

	PHALCON_CALL_FUNCTIONW(NULL, "session_set_save_handler", &callable_open, &callable_close, &callable_read, &callable_write, &callable_destroy, &callable_gc);

	PHALCON_CALL_PARENTW(NULL, phalcon_session_adapter_cache_ce, getThis(), "__construct", options);
}

/**
 *
 * @return boolean
 */
PHP_METHOD(Phalcon_Session_Adapter_Cache, open){

	RETURN_TRUE;
}

/**
 *
 * @return boolean
 */
PHP_METHOD(Phalcon_Session_Adapter_Cache, close){

	RETURN_TRUE;
}

/**
 *
 * @param string $sessionId
 * @return mixed
 */
PHP_METHOD(Phalcon_Session_Adapter_Cache, read){

	zval *sid, lifetime = {}, cache = {};

	phalcon_fetch_params(0, 1, 0, &sid);

	phalcon_read_property(&lifetime, getThis(), SL("_lifetime"), PH_NOISY);
	phalcon_read_property(&cache, getThis(), SL("_cache"), PH_NOISY);

	PHALCON_RETURN_CALL_METHODW(&cache, "get", sid, &lifetime);
}

/**
 *
 * @param string $sessionId
 * @param string $data
 */
PHP_METHOD(Phalcon_Session_Adapter_Cache, write){

	zval *sid, *data, lifetime = {}, cache = {};

	phalcon_fetch_params(0, 2, 0, &sid, &data);

	phalcon_read_property(&lifetime, getThis(), SL("_lifetime"), PH_NOISY);
	phalcon_read_property(&cache, getThis(), SL("_cache"), PH_NOISY);

	PHALCON_CALL_METHODW(NULL, &cache, "save", sid, data, &lifetime);	
}

/**
 *
 * @param string $session_id optional, session id
 *
 * @return boolean
 */
PHP_METHOD(Phalcon_Session_Adapter_Cache, destroy){

	zval *_sid, sid, cache = {};

	phalcon_fetch_params(0, 0, 1, &_sid);

	if (!_sid) {
		PHALCON_CALL_SELFW(&sid, "getid");
	} else {
		PHALCON_CPY_WRT(&sid, _sid);
	}

	phalcon_read_property(&cache, getThis(), SL("_cache"), PH_NOISY);

	PHALCON_RETURN_CALL_METHODW(&cache, "delete", &sid);
}

/**
 *
 * @return boolean
 */
PHP_METHOD(Phalcon_Session_Adapter_Cache, gc){

	RETURN_TRUE;
}
