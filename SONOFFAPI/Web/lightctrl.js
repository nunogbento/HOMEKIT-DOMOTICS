'use strict';

angular.module('SONOFFSERVER.LightCtrl', ['toggle-switch'])
    .controller('LightController', function ($scope, $http) {
        // Initialize the model
       
		$scope.Lights = new Array();
		$scope.Lights[0] = {
			Address: 12,
			Name: 'Power',
			Value: false,
			onChange: function () {
				$http({
					method: "get",
					url: this.Value == 0 ? "/turnoff?address="+this.Address : "/turnon?address="+this.Address,
					headers: {
						'Content-Type': 'application/x-www-form-urlencoded; charset=utf-8'
					}
				});
			},
		};
		
    }).factory(
        "transformRequestAsFormPost",
        function () {
            // I prepare the request data for the form post.
            function transformRequest(data, getHeaders) {
                if (!angular.isObject(data)) {
                    return ((data == null) ? "" : data.toString());
                }
                var buffer = [];
                // Serialize each key in the object.
                for (var name in data) {
                    if (!data.hasOwnProperty(name)) {
                        continue;
                    }
                    var value = data[name];
                    buffer.push(
                        encodeURIComponent(name) +
                        "=" +
                        encodeURIComponent((value == null) ? "" : value)
                    );
                }
                // Serialize the buffer and clean it up for transportation.
                var source = buffer
                    .join("&")
                    .replace(/%20/g, "+");
                return (source);
            }
            // Return the factory value.
            return (transformRequest);
        }
    );
