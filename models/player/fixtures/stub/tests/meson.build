# ==============================================================================
# Suite de CONTRATO do stub -- forma do artefato, nao comportamento.
#
# 'stub_lib' e 'sdk_dep'/'mixr_dep' vem do escopo do meson.build pai (Meson
# compartilha o namespace entre um project() e os subdir() dele) -- nao sao
# redeclarados aqui.
# ==============================================================================
check_contract = find_program('check_contract.sh')

test('contrato-simbolo-unico-e-deps-resolvidas', check_contract,
    args    : [stub_lib],
    suite   : 'contract',
    timeout : 30,
)
