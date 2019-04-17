# Data

## Reference Data

The reference data lists all devices within their respective resource-categorization.
See also [Topic Namespace and Templating][topic-templating] for further details.
Each topic contains the resource-categorization and the device-id, for example, for "device-1 Nickname" the resource categorization would be as follows:

``region-1-unique-id/location-1-unique-id/prod-line-1-unique-id``

This resource categorization must be configured for each device.

````
{
  "regions": [
    {
      "name": "region-1 Nickname",
      "id": "region-1-unique-id",
      "locations": [
        {
          "name": "location-1 Nickname",
          "id": "location-1-unique-id",
          "productionLines": [
            {
              "name": "prod-line-1 Nickname",
              "id": "prod-line-1-unique-id",
              "devices": [
                {
                  "name": "device-1 Nickname",
                  "id": "device-1-unique-id"
                },
                {
                  "name": "device-2 Nickname",
                  "id": "device-2-unique-id"
                }
              ]
            },

... and many more ...
````

----------
The End.

[topic-templating]: https://github.com/solace-iot-team/Bosch-IoT-Showcase/wiki/topic-templating
