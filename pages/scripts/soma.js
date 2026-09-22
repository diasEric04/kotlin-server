window.addEventListener("load", () => {
    let num1 = parseFloat(prompt("insira o primeiro numero: "))
    let num2 = parseFloat(prompt("insira o segundo numero: "))
    const resultado = num1 + num2;
    document.getElementById("resultado").innerHTML += `resultado da soma entre ${num1} e ${num2} é ${resultado}`
})