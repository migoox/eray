float3 phongDirLight(float3 albedo, float3 lightDir, float3 cameraPos, float3 fragPos, float3 normal) {
  // Light colors settings
  const float3 ambientColor  = float3(1.0, 0.95, 0.85);
  const float3 diffuseColor  = float3(1.00, 0.93, 0.88);
  const float3 specularColor = float3(1.0, 0.3, 0.3);

  const float3 backDiffuseColor = float3(1.0, 0.1, 0.1);

  // Material settings
  const float ka   = 0.2;
  const float kd   = 0.35;
  const float ks   = 0.5;
  const float kpow = 64.;

  const float kdb = 0.35;

  lightDir             = -normalize(lightDir);  // The light dir must point from frag pos
  const float3 viewDir = normalize(cameraPos - fragPos);

  float3 ambient = ka * ambientColor;

  float diffuseIntensity = saturate(dot(normal, lightDir));
  float3 diffuse         = kd * diffuseIntensity * diffuseColor;

  float3 half_vec         = normalize(lightDir + viewDir);
  float specularIntensity = pow(saturate(dot(normal, half_vec)), kpow);
  float3 specular         = ks * specularIntensity * specularColor;

  return (ambient + diffuse) * albedo + specular;
}
