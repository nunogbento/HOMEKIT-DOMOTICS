'use strict';

angular.module('MORDOMUSSERVER.LightCtrl', ['toggle-switch'])
    .controller('LightController', function ($scope, $http, transformRequestAsFormPost) {
        // Initialize the model
        $http.get('./layout.json')
            .then(function (res) {
                $scope.Lights = new Array();

                for (var i = 0; i < res.data.length; i++) {
                    
                    $scope.Lights[i] = {
                        Address: res.data[i].A,
                        Name: res.data[i].N,
                        Value: false,
                        onChange: function () {
                            $http({
                                method: "post",
                                url: this.Value == 0 ? "turnoutputoff" : "turnoutputon",
                                headers: {
                                    'Content-Type': 'application/x-www-form-urlencoded; charset=utf-8'
                                },
                                transformRequest: transformRequestAsFormPost,
                                data: {
                                    address: this.Address
                                }
                            });
                        },

                    };


                };
            }

        );
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
