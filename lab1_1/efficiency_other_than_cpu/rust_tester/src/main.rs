use core::{slice, task};
use std::io::{self, BufRead};
use std::time::{self, Instant};
fn task1()
{
    let mut buffer = String::new();
    io::stdin().read_line(&mut buffer);
    let n : usize= buffer.trim().parse().unwrap(); buffer.clear();
    io::stdin().read_line(&mut buffer);
    let mut arr : Vec<f32> = buffer.trim().split_whitespace().map(|s| s.parse::<f32>().unwrap()).collect();
    assert_eq!(arr.len(), n, "{} {}", arr.len(), n);
    let start = Instant::now();
    quick_sort(&mut arr[..]);
    let duration = start.elapsed();
    println!("Task1 qsort Time:{}", duration.as_secs_f32());
}

fn task2()
{
    let mut buffer = String::new();
    io::stdin().read_line(&mut buffer);
    let params : Vec<usize>= buffer.trim().split_whitespace().map(|s| s.parse::<usize>().unwrap()).collect();
    assert_eq!(params.len(), 3);
    let N = params[0];
    let M = params[1];
    let K = params[2];
    buffer.clear();
    io::stdin().read_line(&mut buffer);
    let A : Vec<f32> = buffer.trim().split_whitespace().map(|s| s.parse::<f32>().unwrap()).collect();
    buffer.clear();    io::stdin().read_line(&mut buffer);
    let B : Vec<f32> = buffer.trim().split_whitespace().map(|s| s.parse::<f32>().unwrap()).collect();
    let mut C = vec![0_f32; N * K];
    let start = Instant::now();
    mat_mul(&A[..], &B[..], &mut C[..], N, M, K);
    let duration = start.elapsed();
    println!("Task2 matmul Time:{}", duration.as_secs_f32());
}

fn task3()
{
    let mut buffer = String::new();
    io::stdin().read_line(&mut buffer);
    let params : Vec<usize>= buffer.trim().split_whitespace().map(|s| s.parse::<usize>().unwrap()).collect();
    assert_eq!(params.len(), 2);
    let M = params[0];
    let N = params[1];
    let start = Instant::now();
    Ackermann(M, N);
    let duration = start.elapsed();
    println!("Task3 ackermann Time:{}", duration.as_secs_f32());
}

fn quick_sort(arr: &mut [f32]) {
    if arr.len() <= 1 {
        return;
    }
    let pivot_index = arr.len() / 2;
    let pivot = arr[pivot_index];
    let mut i = 0; let mut j = arr.len() - 1;
    while i <= j
    {
        while arr[i] < pivot
        {
            i = i + 1;
        }
        while arr[j] > pivot
        {
            j = j - 1;
        }
        if i <= j
        {
            arr.swap(i, j);
            i = i + 1;
            j = j - 1;
        }
    }
    quick_sort(&mut arr[0..j]);
    quick_sort(&mut arr[i..]);
}

fn mat_mul(A: &[f32], B: &[f32], C: &mut[f32], n : usize, m: usize, k: usize)
{
    for i in 0..n
    {
        for j in 0..k
        {
            C[i * k + j] = 0.;
            for l in 0..m
            {
                C[i * k + j] += A[i * m + l] * B[l * k + j];
            }
        }
    }
}

fn Ackermann(m: usize, n:usize) -> usize
{
    if m == 0
    {
        return n + 1;
    }
    if n == 0
    {
        return Ackermann(m - 1, 1);
    }
    return Ackermann(m - 1, Ackermann(m, n - 1));
}

fn main() {
    task1();
    task2();
    task3();
}
