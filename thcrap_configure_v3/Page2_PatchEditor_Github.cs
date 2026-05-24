using Newtonsoft.Json;
using System.Collections.Generic;
using System.Net.Http;
using System.Threading.Tasks;
using thcrap_cs_lib;

namespace thcrap_configure_v3
{
    class Page2_PatchEditor_Github
    {
        private HttpClient http = null;
        public string token = null;

        private async Task<T> MakeHttpPostRequest<T>(string url, Dictionary<string, string> postParams)
        {
            if (http == null)
            {
                http = new HttpClient();
                http.DefaultRequestHeaders.Add("Accept", "application/json");
            }
            var encodedParams = new FormUrlEncodedContent(postParams);
            var response = await http.PostAsync(url, encodedParams);
            var json = await response.Content.ReadAsStringAsync();
            return JsonConvert.DeserializeObject<T>(json);
        }

        public class DeviceFlowParams
        {
            public string device_code { get; set; }
            public string user_code { get; set; }
            public string verification_uri { get; set; }
            public int interval { get; set; }
        }

        public async Task<DeviceFlowParams> StartDeviceFlow()
        {
            return await MakeHttpPostRequest<DeviceFlowParams>("https://github.com/login/device/code", new Dictionary<string, string>
            {
                { "client_id", "Iv23liI6p1Rjdzvm56DD" },
                { "scope", "repo" }
            });
        }

        public class TokenResponse
        {
            public string access_token { get; set; }
            public string error { get; set; }
        }

        public async Task<bool> GetToken(string device_code, int requests_interval)
        {
            while (true)
            {
                await Task.Delay(requests_interval * 1000);

                var response = await MakeHttpPostRequest<TokenResponse>("https://github.com/login/oauth/access_token", new Dictionary<string, string>
                {
                    { "client_id", "Iv23liI6p1Rjdzvm56DD" },
                    { "device_code", device_code },
                    { "grant_type", "urn:ietf:params:oauth:grant-type:device_code" }
                });

                if (!string.IsNullOrEmpty(response.access_token))
                {
                    token = response.access_token;
                    return true;
                }
                else
                {
                    if (response.error == "authorization_pending")
                        continue;
                    ThcrapDll.log_print("OAuth error: " + response.error);
                    return false;
                }
            }
        }
    }
}
