def max_valor_placa(x, y, pecas):
    # Inicializa uma matriz para armazenar os valores máximos
    dp = [[0] * (y + 1) for _ in range(x + 1)]

    # Itera sobre as diferentes dimensões da placa
    for i in range(1, x + 1):
        for j in range(1, y + 1):
            # Itera sobre as peças disponíveis
            for peca in pecas:
                xp, yp, valor = peca

                # Verifica se a peça pode ser colocada na placa
                if xp <= i and yp <= j:
                    dp[i][j] = max(dp[i][j], valor + dp[i][j - yp] + dp[i- xp][yp], valor + dp[xp][j -yp] + dp[i - xp][j])

    # O valor máximo estará armazenado em dp[x][y]
    return dp[x][y]

# Exemplo de uso
placa_x = 5
placa_y = 8
pecas = [(2, 3, 10), (1, 2, 5), (3, 4, 15)]

resultado = max_valor_placa(placa_x, placa_y, pecas)
print("O valor máximo que pode ser obtido é:", resultado)
