def calc(params):
    mean = sum(params) / len(params) 
    return sum((i - mean) ** 2 for i in params) / len(params)
