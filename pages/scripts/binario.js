window.addEventListener("load", () => {
    let num = parseFloat(prompt("insira o numero para saber seu binário: "));
    let decimal = num;
    let binario = "";
    while (num > 0) {
        let ultimoBinario = num % 2 == 0 ? "0" : "1";
        num = parseInt(num / 2);
        binario += ultimoBinario;  
    }
    
    document.getElementById("resultado").innerHTML += `o binario de ${decimal} é ${binario.split("").reverse().join("")}`
})