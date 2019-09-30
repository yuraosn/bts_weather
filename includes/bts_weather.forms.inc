<?php

/**
 * @file
 * Forms module.
 *
 * File name: bts_weather.forms.inc
 * Date: 29.09.19
 * Developer: http://BuyTopSite.com
 */

/**
 * Menu callback; Form settings module.
 *
 * @param array $form
 * @param array $form_state
 *
 * @return mixed
 *
 * @see bts_weather_menu()
 */
function bts_weather_settings_form($form, &$form_state) {
  $form['note'] = array(
    '#type' => 'fieldset',
    '#title' => t('note'),
    '#collapsed' => FALSE,
    '#collapsible' => FALSE,
    '#description' => t('To display the weather in the block on the site, you need to configure the module.
<br>Indicate access to the service that provides the weather. Set up caching and the default city for which the weather will be displayed for users.
<br>You can configure and display a block for displaying weather !link_block_settings.<br>To automatically determine the current city, you can install an additional module: "smart_ip"', array(
      '!link_block_settings' => l(t('here'), 'admin/structure/block/manage/bts_weather/weather/configure'),
    )),
  );

  $form['bts_weather_default_location'] = array(
    '#type' => 'select',
    '#title' => t('Location default'),
    '#options' => array(
      'city' => t('Specific location'),
    ),
    '#empty_option' => t('- None -'),
    '#default_value' => variable_get('bts_weather_default_location', ''),
    '#description' => t('The default location for displaying weather.'),
  );
  if (module_exists('smart_ip')) {
    $form['bts_weather_default_location']['#options']['smart_ip'] = t('From IP user (smart_ip)');
  }
  $form['bts_weather_default_location_city'] = array(
    '#type' => 'textfield',
    '#title' => t('Location default'),
    '#autocomplete_path' => 'bts-weather/autocomplete',
    '#default_value' => variable_get('bts_weather_default_location_city', ''),
    '#states' => array(
      'visible' => array(
        'select[name="bts_weather_default_location"]' => array('value' => 'city'),
      ),
      'required' => array(
        'select[name="bts_weather_default_location"]' => array('value' => 'city'),
      ),
    ),
    '#element_validate' => array('element_validate_bts_weather_location_id'),
    '#description' => t('Select a city to display the default weather for it on the site.'),
  );
  $form['bts_weather_apikey'] = array(
    '#type' => 'textfield',
    '#title' => t('API keys'),
    '#maxlength' => 35,
    '#size' => 35,
    '#description' => t('Specify the token to use the service. Token can be created on the !link_token.', array(
      '!link_token' => l(t('site'), 'https://home.openweathermap.org/api_keys', array('external' => TRUE)),
    )),
    '#default_value' => variable_get('bts_weather_apikey', ''),
  );
  $form['bts_weather_cache_expire'] = array(
    '#type' => 'textfield',
    '#title' => t('Cache expire'),
    '#maxlength' => 6,
    '#size' => 6,
    '#description' => t('Indicate the time during which the weather result will be cached for the selected city.'),
    '#default_value' => variable_get('bts_weather_cache_expire', 60),
    '#field_suffix' => ' ' . t('seconds'),
    '#element_validate' => array('element_validate_integer_positive'),
  );
  $form['bts_weather_count_days'] = array(
    '#type' => 'select',
    '#title' => t('Count forecast days'),
    '#options' => drupal_map_assoc(range(0, 10)),
    '#required' => TRUE,
    '#default_value' => variable_get('bts_weather_count_days', 5),
    '#description' => t('Indicate the number of days displayed for the forecast.'),
  );
  $form['bts_weather_reset_cache'] = array(
    '#type' => 'checkbox',
    '#title' => t('Reset cache'),
    '#default_value' => FALSE,
    '#description' => t('Check the box to clear the cache of saved cities..'),
  );

  $form = system_settings_form($form);

  $form['#submit'][] = 'bts_weather_settings_form_submit';

  return $form;
}

/**
 * Form settings validate.
 *
 * @param array $form
 * @param array $form_state
 *
 * @see bts_weather_settings_form()
 */
function bts_weather_settings_form_validate($form, &$form_state) {
  $values = & $form_state['values'];
  if ($values['bts_weather_default_location'] == 'city' && empty($values['bts_weather_default_location_city'])) {
    form_set_error('bts_weather_default_location_city', t('For the value: "@value", the field: "@field_name" is required.', array(
      '@value' => t('Specific city'),
      '@field_name' => t('Location default'),
    )));
  }
  if (isset($values['bts_weather_reset_cache'])) {
    $form_state['data']['reset_cache'] = $values['bts_weather_reset_cache'];
    unset($values['bts_weather_reset_cache']);
  }
}

/**
 * Form settings submit. Clear cache.
 *
 * @param array $form
 * @param array $form_state
 *
 * @see bts_weather_settings_form()
 */
function bts_weather_settings_form_submit($form, &$form_state) {
  if (isset($form_state['data']['reset_cache']) && !empty($form_state['data']['reset_cache'])) {
    cache_clear_all('bts_weather_request_openweathermap', 'cache');
  }
}


/**
 * Element validate location.
 *
 * @param array $element
 * @param array $form_state
 *
 * @see bts_weather_settings_form()
 * @see bts_weather_block_form()
 */
function element_validate_bts_weather_location_id(&$element, &$form_state) {
  if (!empty($element['#value']) && !bts_weather_autocomplete_parse($element['#value'])) {
    form_error($element, t('Select a city from the proposed options, or leave this field blank.'));
  }
}

/**
 * Form for the weather block.
 *
 * @param array $form
 * @param array $form_state
 *
 * @return mixed
 * @throws \Exception
 *
 * @see bts_weather_block_view()
 */
function bts_weather_block_form($form, &$form_state) {
  // Set geoid for current user.
  if (!isset($_SESSION['geoid']) && variable_get('bts_weather_default_location', '') == 'smart_ip') {
    bts_weather_user_geoid_by_smart_ip(TRUE);
  }
  $form['#attributes']['class'][] = 'bts-weather-wrapper';
  $form['wrapper'] = array(
    '#type' => 'container',
  );
  $form['wrapper']['#prefix'] = '<div id="bts-weather--ajax-wrapper">';
  $form['wrapper']['#suffix'] = '</div>';
  $form['wrapper']['#attached']['css'][] = drupal_get_path('module', 'bts_weather') . '/css/bts-weather.css';
  $geoid = bts_weather_user_geoid();
  if (!$geoid) {
    if (module_exists('smart_ip')) {
      $smart_ip = smart_ip_session_get('smart_ip');
      if (isset($smart_ip["location"]["city"], $smart_ip["location"]["country_code"])
        && !empty($smart_ip["location"]["city"])
        && !empty($smart_ip["location"]["country_code"])
      ) {
        $form['wrapper']['actions']['geoid_my_set'] = array(
          '#type' => 'submit',
          '#value' => t('Get my location'),
          '#submit' => array('bts_weather_block_form_submit_set_by_smart_ip'),
          '#ajax' => array(
            'callback' => 'bts_weather_block_form_rebuild_ajax_callback',
            'wrapper' => 'bts-weather--ajax-wrapper',
          ),
        );
      }
    }
    $form['wrapper']['geoid'] = array(
      '#type' => 'textfield',
      '#title' => t('Search for a location'),
      '#description' => t('Type in a city or country to show for the weather.<br>The city enter only in English!'),
      '#autocomplete_path' => 'bts-weather/autocomplete',
      '#element_validate' => array('element_validate_bts_weather_location_id'),
    );
    $form['wrapper']['actions']['#type'] = 'actions';
    $form['wrapper']['actions']['geoid_set'] = array(
      '#type' => 'submit',
      '#value' => t('Get weather'),
      '#submit' => array('bts_weather_block_form_submit_geoid_set'),
      '#states' => array(
        'visible' => array(
          'input[name="geoid"]' => array('!value' => ''),
        ),
      ),
      '#ajax' => array(
        'callback' => 'bts_weather_block_form_rebuild_ajax_callback',
        'wrapper' => 'bts-weather--ajax-wrapper',
      ),
    );
  }
  else {
    $form['wrapper']['actions']['#type'] = 'actions';
    $form['wrapper']['actions']['geoid_change'] = array(
      '#type' => 'submit',
      '#value' => t('Change weather'),
      '#submit' => array('bts_weather_block_form_submit_geoid_change'),
      '#states' => array(
        'visible' => array(
          'input[name="geoid"]' => array('!value' => ''),
        ),
      ),
      '#ajax' => array(
        'callback' => 'bts_weather_block_form_rebuild_ajax_callback',
        'wrapper' => 'bts-weather--ajax-wrapper',
      ),
    );
    $bts_weather = bts_weather_request_openweathermap('weather', $geoid);
    $bts_forecast = bts_weather_request_openweathermap('forecast', $geoid);
    $form['wrapper']['output'] = array(
      '#type' => 'markup',
      '#markup' => theme('bts_weather_block', array(
        'bts_weather' => $bts_weather,
        'bts_forecast' => $bts_forecast,
      )),
    );
  }

  return $form;
}

/**
 * Form ajax callback for rebuild form.
 *
 * @param array $form
 * @param array $form_state
 *
 * @return array
 *
 * @see bts_weather_block_form()
 */
function bts_weather_block_form_rebuild_ajax_callback($form, &$form_state) {

  return $form['wrapper'];
}

/**
 * Form submit for set the geo ID.
 *
 * @param array $form
 * @param array $form_state
 *
 * @see bts_weather_block_form()
 */
function bts_weather_block_form_submit_geoid_set($form, &$form_state) {
  $form_state['rebuild'] = TRUE;
  $geoid = bts_weather_autocomplete_parse($form_state["values"]["geoid"]);
  if ($geoid) {
    bts_weather_user_geoid(TRUE, $geoid);
  }
}

/**
 * Form submit for change the geo ID.
 *
 * @param array $form
 * @param array $form_state
 *
 * @see bts_weather_block_form()
 */
function bts_weather_block_form_submit_geoid_change($form, &$form_state) {
  $form_state['rebuild'] = TRUE;
  bts_weather_user_geoid(TRUE);
}

/**
 * Form submit for set the geo ID from the smart_ip module.
 *
 * @param array $form
 * @param array $form_state
 *
 * @see bts_weather_block_form()
 * @see bts_weather_user_geoid_by_smart_ip()
 */
function bts_weather_block_form_submit_set_by_smart_ip($form, &$form_state) {
  $form_state['rebuild'] = TRUE;
  bts_weather_user_geoid_by_smart_ip(TRUE);
}
