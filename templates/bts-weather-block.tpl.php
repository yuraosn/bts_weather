<?php

/**
 * @file
 * Template for the weather module.
 *
 * Available:
 * $bts_weather - current weather.
 * $bts_forecast - forecast.
 */
?>
<h2 class="t-a-c">
  <?php print t('Weather in @city, @country_code', array(
    '@city' => $bts_weather["name"],
    '@country_code' => $bts_weather["sys"]["country"],
  )); ?>
</h2>
<div class="t-a-c">
  <img src="https://openweathermap.org/img/wn/<?php print $bts_weather["weather"][0]["icon"]; ?>.png" width="50" height="50">
  <div class="temp"><?php print t('@temp °C', array('@temp' => $bts_weather["main"]["temp"])); ?></div>
</div><p class="t-a-c"><?php print $bts_weather["weather"][0]["description"]; ?> | <?php print t('@wind_speed m/s', array(
    '@wind_speed' => $bts_weather["wind"]["speed"],
  )); ?> | <?php print $bts_weather["main"]["humidity"]; ?> %</p>
<?php if (isset($bts_forecast) && !empty($bts_forecast["list"])): ?>
  <table>
    <tbody>
    <?php foreach ($bts_forecast["list"] as $bts_f): ?>
      <tr>
        <td><?php print date('d.m.Y', $bts_f["dt"]); ?></td>
        <td><?php print t('@temp °C', array('@temp' => round($bts_f["main"]["temp"]))); ?> | <?php print t('@wind_speed m/s', array(
            '@wind_speed' => round($bts_f["wind"]["speed"]),
          )); ?>
          <br><?php print $bts_f["weather"][0]["description"]; ?>
      </tr>
    <?php endforeach; ?>
    </tbody>
  </table>
<?php endif; ?>
