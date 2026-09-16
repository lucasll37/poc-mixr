# Fixtures de `.onnx` para o teste do schema variável

Três arquivos, todos com pesos **aleatórios** (não treinados — servem só para
exercitar a cadeia de carga/forma/inferência/metadados de `test_onnx_nodes.cpp`,
nunca para voar de verdade). Gerados por
`src/poc/rl-training/tools/export_onnx.py` (que grava a metadata
`xrlbridge.fields` no próprio arquivo — ver o comentário do módulo).

Não instalados (ficam fora de `configs/`, que é o que `install_data()`
publica) — são dado de teste, não de produção.

## Regenerar

Precisa de um venv com `onnx`+`numpy` (ver `src/poc/rl-training/requirements.txt`)
e do módulo nativo já instalado (`make install` na raiz, uma vez).

```bash
python3 src/poc/rl-training/tools/export_onnx.py --random --fields all \
  -o models/players/air/A-4/tests/native/fixtures/policy_all38.onnx

python3 src/poc/rl-training/tools/export_onnx.py --random --fields "northM eastM altitudeM" \
  -o models/players/air/A-4/tests/native/fixtures/policy_3fields.onnx

python3 src/poc/rl-training/tools/export_onnx.py --random \
  -o models/players/air/A-4/tests/native/fixtures/policy_classic28_metadata_divergente.onnx
```

O terceiro (`policy_classic28_metadata_divergente.onnx`) precisa de um passo a
mais: depois de exportado (28 entradas, metadata correta), a metadata
`xrlbridge.fields` é reescrita à mão trocando a ordem dos dois primeiros nomes
— mesma contagem, mesmo conjunto, ordem diferente da que o schema `"classic28"`
resolve. É o que prova que `OnnxPolicyAction`/`OnnxScoreCondition` checam
IDENTIDADE (via `xinfer::fields()`), não só contagem:

```python
import onnx
caminho = "models/players/air/A-4/tests/native/fixtures/policy_classic28_metadata_divergente.onnx"
modelo = onnx.load(caminho)
for prop in modelo.metadata_props:
    if prop.key == "xrlbridge.fields":
        nomes = prop.value.split(",")
        nomes[0], nomes[1] = nomes[1], nomes[0]
        prop.value = ",".join(nomes)
onnx.save(modelo, caminho)
```
