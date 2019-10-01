<?php

/**
 * @file
 * List of auxiliary functions for the module.
 *
 * File name: bts_weather.fn.inc
 * Date: 29.09.19
 * Developer: http://BuyTopSite.com
 */

/**
 * Get/set geo ID for the "smart_ip" module.
 *
 * @param bool $set
 *
 * @return int|null
 *  Return a geoid.
 */
function bts_weather_user_geoid_by_smart_ip(bool $set = FALSE) {
  $geoid = NULL;
  if (module_exists('smart_ip')) {
    $smart_ip_session = smart_ip_session_get('smart_ip');
    // Check that the lookup did find a location for the IP.
    if (isset($smart_ip_session['location'])) {
      $city = $smart_ip_session["location"]["city"];
      $country_code = $smart_ip_session["location"]["country_code"];
      if (!empty($city) and !empty($country_code)) {
        $sql = db_select('bts_weather', 'bw')
          ->fields('bw', array('geoid'))
          ->orderBy('name', 'ASC');
        $or = db_and()
          ->where('UPPER(country) = UPPER(:country_code)', array(':country_code' => $country_code))
          ->where('UPPER(name) = UPPER(:city)', array(':city' => $city));
        $sql->condition($or);
        $sql->range(0, 1);
        $result = $sql->execute()->fetchField();
        if ($result) {
          $geoid = $result;
          if ($set) {
            bts_weather_user_geoid(TRUE, $result);
          }
        }
      }
    }
  }

  return $geoid;
}

/**
 * Get/set the Geo ID from/to session of the current user.
 *
 * @param bool $set
 * @param string $geoid
 *
 * @return bool|mixed
 */
function bts_weather_user_geoid(bool $set = FALSE, $geoid = '') {
  $output = FALSE;
  if ($set) {
    if ($geoid) {
      $_SESSION['geoid'] = $geoid;
    }
    elseif (isset($_SESSION['geoid'])) {
      $_SESSION['geoid'] = '';
    }
    $output = TRUE;
  }
  elseif (isset($_SESSION['geoid']) && !empty($_SESSION['geoid'])) {
    $output = $_SESSION['geoid'];
  }

  return $output;
}

/**
 * Menu callback; Retrieve a JSON object containing autocomplete suggestions
 * for city.
 *
 * @param string $string
 *
 * @return string JSON.
 */
function bts_weather_search_autocomplete(string $string = '') {
  $string = str_replace(' ', '_', $string);
  $matches = array();
  $sql = db_select('bts_weather', 'bw')
    ->fields('bw', array('geoid', 'name', 'country'))
    ->orderBy('name', 'ASC');
  $or = db_or()
    ->where('UPPER(country) LIKE UPPER(:search)', array(':search' => "%$string%"))
    ->where('UPPER(name) LIKE UPPER(:search)', array(':search' => "%$string%"));
  $sql->condition($or);
  $sql->range(0, 10);
  $result = $sql->execute();

  foreach ($result as $item) {
    $matches[$item->name . ' (' . $item->country . ')' . ' [geoid:' . $item->geoid . ']'] = $item->name . ' (' . $item->country . ')';
  }

  drupal_json_output($matches);
}

/**
 * Parse string. Get the Geo ID from string autocomplete.
 *
 * @param string $string
 *
 * @return bool|mixed
 *
 * @see bts_weather_search_autocomplete()
 */
function bts_weather_autocomplete_parse(string $string) {
  $output = FALSE;
  if ($string) {
    preg_match('/\[geoid:(\d+)\]/', $string, $preg_matches);
    if ($preg_matches && !empty($preg_matches[1])) {

      return $preg_matches[1];
    }
  }

  return $output;
}

/**
 * Menu callback; Retrieve a JSON object containing autocomplete suggestions
 * for city.
 *
 * @TODO поиск города напрямую из openweathermap можно вводить города по
 *   русски, как альтернатива можно использовать.
 *
 * @param string $search
 *
 * @return string JSON.
 */
function bts_weather_search_autocomplete_from_sourse(string $search = '') {
  $datas = bts_weather_request_openweathermap('find', $search);
  if ($datas && is_array($datas)) {
    foreach ($datas as $city) {
      $name = check_plain($city->name);
      $datas[$name . ' [geoid:' . $city->geoid . ']'] = $name;
    }
  }

  drupal_json_output($datas);
}

/**
 * Get weather from the openweathermap site and cache.
 *
 * @param string $type type request
 *   available: weather, forecast, daily or find.
 * @param string|int $search Location ID or city (for $type == find)
 * @param bool $reset_cache
 *   true - reset cache for current write.
 *
 * @return bool
 */
function bts_weather_request_openweathermap(string $type, $search, $reset_cache = FALSE) {
  // Check the type.
  if (!in_array($type, array('weather', 'forecast', 'find', 'daily'))) {
    return FALSE;
  }

  $key = $type . '-' . $search;
  if ($key) {
    $data = &drupal_static(__FUNCTION__);
    if (!isset($data[$key]) || $reset_cache) {
      $cache = cache_get(__FUNCTION__);
      $expire = variable_get('bts_weather_cache_expire', 60);
      $time = REQUEST_TIME;
      if (!$reset_cache
        && $cache && isset($cache->data[$key]['created'])
        && ($cache->data[$key]['created'] + $expire) > $time
      ) {
        $data[$key] = $cache->data[$key]['data'];
      }
      else {
        $url = 'http://api.openweathermap.org/data/2.5/' . $type;
        $params = array(
          'units' => 'metric',
          'lang' => 'ru',
          'APPID' => variable_get('bts_weather_apikey', ''),
        );
        switch ($type) {
          case 'forecast':
          case 'weather':
            $params['id'] = $search;
            $params['cnt'] = variable_get('bts_weather_count_days', 5);
            break;

          case 'daily':
            $params['id'] = $search;
            $params['cnt'] = variable_get('bts_weather_count_days', 5);
            unset($params['units'], $params['lang']);
            break;

          case 'find':
            $params['q'] = $search;
            break;
        }
        $url .= '?' . drupal_http_build_query($params);
        $options = array(
          'timeout' => 10,
        );
        $response = drupal_http_request($url, $options);

        $datas = array();
        $result = NULL;
        if (isset($response->error) && !empty($response->error)) {
          watchdog('bts_weather', 'An error occurred while accessing the site: api.openweathermap.org error: @error', array(
            '@error' => $response->error,
          ), WATCHDOG_ERROR);
        }
        elseif (!empty($response->data)) {
          $response = drupal_json_decode($response->data);
          switch ($type) {
            case 'forecast':
            case 'weather':
            case 'daily':
              $result = $response;
              break;

            case 'find':
              if (!empty($response['list'])) {
                $result = $response['list'];
              }
              break;
          }
        }
        if (!is_null($result)) {
          $cache->data[$key] = array();
          $cache->data[$key]['data'] = $data[$key] = $result;
          $cache->data[$key]['created'] = $time;
          cache_set(__FUNCTION__, $cache->data);
        }
      }
    }

    return $data[$key];
  }

  return FALSE;
}