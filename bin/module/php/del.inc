#!/bin/sh

hestia_module_php_del() {
    source $HESTIA/bin/module/func.inc

    module_installed=$(hestia_module_isinstalled php)
    if [ ! "$module_installed" ] && [ ! "$param_force" ]; then
        echo "PHP module is not installed. See 'hestia module info php'."
        exit 1
    fi

    module_variant=$(hestia_module_getvariant php)
    if [ "$module_variant" != 'php-fpm' ] && [ ! "$param_force" ]; then
        echo "The installed PHP module is not FPM. See 'hestia module info php'."
        exit 1
    fi

    # Verify php version format
    if [[ ! $param_ver =~ ^[0-9]\.[0-9]+ ]]; then
        echo "The PHP version format is invalid, it should look like [0-9].[0-9]..."
        exit
    fi

    # Check version is supported
    case $param_ver in
        5.6|7.0|7.1|7.2|7.3|7.4)
            php_version=$param_ver
            ;;
        *)
            echo "PHP version $param_ver is not supported."
            exit 1
    esac

    php_withoutdot=${php_version//.}

    php_version_present=$(osal_kv_read_bool $HESTIA_CONF_MODULES/php.conf "php${php_withoutdot}_present")
    if [ ! "$php_version_present" ] && [ ! "$param_force" ]; then
        echo "PHP version ${php_version} is not present. See 'hestia module php list'."
        exit 1
    fi

    # Perform verification if read-only mode is enabled
    check_hestia_demo_mode

    echo "Removing PHP version ${php_version}..."

    php_etc_folder=$(osal_multiphp_etc_folder $php_version)
    hestia_config_backup 'php-del' $php_etc_folder \
        $HESTIA/data/templates/web/php-fpm/PHP-${php_version/\./_}.tpl

    php_prefix=$(osal_multiphp_php_package_prefix $php_version)

    mph="$php_prefix-mbstring $php_prefix-bcmath $php_prefix-cli $php_prefix-curl
        $php_prefix-fpm $php_prefix-gd $php_prefix-intl $php_prefix-mysql
        $php_prefix-soap $php_prefix-xml $php_prefix-zip $php_prefix-mbstring
        $php_prefix-json $php_prefix-bz2 $php_prefix-pspell"

    # Check is version is 7.1 or below to add mcrypt
    if [[ `echo "$php_version 7.2" | awk '{print ($1 < $2)}'` == 1 ]]; then
        mph="$mph $php_prefix-mcrypt"
    fi

    # Purge php packages
    osal_service_stop $php_prefix-fpm > /dev/null 2>&1
    osal_service_disable $php_prefix-fpm > /dev/null 2>&1
    osal_execute_with_spinner osal_package_remove $mph

    # Check if uninstallation was sucessfully
    if [ $(osal_multiphp_fpm_isinstalled $php_version) ]; then
        echo "Uninstallation failed, please run the following command manualy for debuging:"
        echo "$OSAL_CMD_PACKAGE_MANAGER (remove or purge) ${mph//\\n/ \\}"
    fi

    # Cleanup files and folders
    [ -f $HESTIA/data/templates/web/php-fpm/PHP-${php_version/\./_}.tpl ] && rm -f $HESTIA/data/templates/web/php-fpm/PHP-${php_version/\./_}.tpl
    [[ -d "$php_etc_folder" ]] && rm -rf "$php_etc_folder"

    osal_kv_write $HESTIA_CONF_MODULES/php.conf "php${php_withoutdot}_present" '0'

    log_history "removed php $phpversion" '' 'admin'

    exit 0
}